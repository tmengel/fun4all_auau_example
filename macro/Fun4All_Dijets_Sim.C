#ifndef FUN4ALL_DIJETS_SIM_C
#define FUN4ALL_DIJETS_SIM_C

// Embedded (PYTHIA jets in HIJING) Au+Au simulation DST -> analysis TTree "T".
//
//   input DST (calibrated towers, MBD, global vertex, truth jets, HepMC)
//     -> RetowerCEMC                      EMCal on the HCal tower grid
//     -> MinimumBiasClassifier, CentralityReco
//     -> EventSelector( MinBiasCut )      drop non-minimum-bias events
//     -> [ HijingTowerLoader ]            optional: HIJING-only towers
//     -> JetReco                          unsubtracted R = 0.3 tower jets
//     -> DetermineTowerRho x3             multiplicity rho per calorimeter layer
//     -> TowerRhoToBackground             rho -> UE per layer and eta ring
//     -> SubtractTowers                   rho-subtracted towers
//     -> JetReco                          R = 0.3 jets AntiKt_TowerInfo_r03_Rho
//     -> AnaTreev1                        the TTree
//
// Everything except libmyana (src/myana of this repository) is coresoftware.
// The rho-based UE subtraction is coresoftware PR #4465; until it is in a
// release, source setup_rho.sh (SDCC) to get a build that has it.
//
// Setup:
//   source ../setup_rho.sh    (then build src/myana, see README.md)
//   root -l -q -b 'Fun4All_Dijets_Sim.C(100)'

#include <GlobalVariables.C>

#include <HIJetReco.C>  // HIJETS::GetFJAlgo

#include <fun4all/Fun4AllDstInputManager.h>
#include <fun4all/Fun4AllServer.h>

#include <ffamodules/CDBInterface.h>
#include <ffamodules/FlagHandler.h>

#include <phool/recoConsts.h>

#include <globalvertex/GlobalVertex.h>

#include <centrality/CentralityReco.h>

#include <calotrigger/MinimumBiasClassifier.h>

#include <jetbase/JetReco.h>
#include <jetbase/TowerJetInput.h>

#include <jetbackground/DetermineTowerRho.h>
#include <jetbackground/RetowerCEMC.h>
#include <jetbackground/SubtractTowers.h>
#include <jetbackground/TowerRho.h>

#include <jetbackground/TowerRhoToBackground.h>

#include <myana/AnaTreev1.h>
#include <myana/EventSelector.h>
#include <myana/HijingTowerLoader.h>
#include <myana/MinBiasCut.h>

#include <TSystem.h>

#include <iostream>
#include <string>
#include <utility>
#include <vector>

R__LOAD_LIBRARY(libfun4all.so)
R__LOAD_LIBRARY(libffamodules.so)
R__LOAD_LIBRARY(libglobalvertex.so)
R__LOAD_LIBRARY(libcentrality.so)
R__LOAD_LIBRARY(libcalotrigger.so)
R__LOAD_LIBRARY(libjetbase.so)
R__LOAD_LIBRARY(libjetbackground.so)
R__LOAD_LIBRARY(libmyana.so)

void Fun4All_Dijets_Sim(
    const int nEvents = 10,
    const std::string &infile = "/sphenix/tg/tg01/jets/tmengel/ppg14/sim_scaling/08_14_2026_v001/dsts/jet30_hijing/scaled/DST_SCALED_jet30_hijing31_pass2-00100.root",
    const std::string &outfile = "sim_output_ana.root",
    // jet sample: truth jets and the HepMC record are saved; false for minimum bias HIJING
    const bool is_jet_sample = true,
    // optional TTree with the HIJING-only towers of the same segment (see
    // HijingTowerLoader.h), splits the jet energy into PYTHIA and HIJING; "" = no split
    const std::string &hijingfile = "",
    // scale applied to the HIJING towers of the DST (1.11 for the UE-scaled sample)
    const float hijing_scale = 1.11,
    // TowerRhoToBackground eta calibration and DetermineTowerRho tower shape
    // payloads, default: UE-scaled HIJING sample
    const std::string &rho_ue_payload = "/sphenix/user/tmengel/jstg03/calibrations/rho_ue_calib_prod_v1/mb_hijing31_scaled_v1/rho_ue_payload.root",
    const std::string &rho_smap_payload = "/sphenix/user/tmengel/jstg03/calibrations/rho_ue_calib_prod_v1/mb_hijing31_scaled_v1/smap_payload.root",
    // subtract only the UE of the live area of each retowered EMCal tower
    const bool rho_ue_live_fraction = true)
{
  const std::string tower_prefix = "TOWERINFO_CALIB";
  const std::string hijing_prefix = "TOWERINFO_HIJING";
  const std::string rho_sub_prefix = "TOWERINFO_RHOSUB";
  const float jet_R = 0.3;

  // sim centrality calibrations (MinimumBiasClassifier / CentralityReco overwrites)
  const std::string centrality_dir = "/sphenix/user/dlis/Projects/centrality/cdb/calibrations";

  // tower jet input with the MBD vertex
  auto TowerInput = [](const Jet::SRC src, const std::string &prefix)
  {
    auto *in = new TowerJetInput(src, prefix);
    in->set_GlobalVertexType(GlobalVertex::MBD);
    return in;
  };

  // EMCal retowered to the HCal grid; dead area is neither masked nor rescaled
  auto Retower = [](const std::string &name, const std::string &prefix)
  {
    auto *rcemc = new RetowerCEMC(name);
    rcemc->set_towerinfo(true);
    rcemc->set_frac_cut(1.0);
    rcemc->set_do_rescale(false);
    rcemc->set_towerNodePrefix(prefix);
    return rcemc;
  };

  Enable::VERBOSITY = 0;
  Enable::CDB = true;

  auto *se = Fun4AllServer::instance();
  se->Verbosity(1);

  auto *rc = recoConsts::instance();
  rc->set_StringFlag("CDB_GLOBALTAG", "MDC2");
  rc->set_uint64Flag("TIMESTAMP", 31);  // run 31: HIJING Au+Au embedding
  CDBInterface::instance()->Verbosity(Enable::VERBOSITY);

  se->registerSubsystem(new FlagHandler());

  auto *input = new Fun4AllDstInputManager("DSTINPUT");
  input->AddFile(infile);
  se->registerInputManager(input);

  // TOWERINFO_CALIB_CEMC_RETOWER
  se->registerSubsystem(Retower("RetowerCEMC", tower_prefix));

  // minimum bias and centrality
  auto *mb = new MinimumBiasClassifier("MinimumBiasClassifier");
  mb->setIsSim(true);
  mb->setOverwriteScale(centrality_dir + "/scales/cdb_centrality_scale_1.root");
  mb->setOverwriteVtx(centrality_dir + "/vertexscales/cdb_centrality_vertex_scale_1.root");
  se->registerSubsystem(mb);

  auto *cr = new CentralityReco("CentralityReco");
  cr->setOverwriteScale(centrality_dir + "/scales/cdb_centrality_scale_1.root");
  cr->setOverwriteVtx(centrality_dir + "/vertexscales/cdb_centrality_vertex_scale_1.root");
  cr->setOverwriteDivs(centrality_dir + "/divs/cdb_centrality_1.root");
  se->registerSubsystem(cr);

  // events failing a cut are aborted here and never reach the tree
  auto *es = new EventSelector("EventSelector");
  auto *mbc = new MinBiasCut();
  mbc->SetNodeName("MinimumBiasInfo");
  es->AddCut(mbc);
  es->PrintCuts();
  se->registerSubsystem(es);

  // HIJING-only towers, retowered like the jet input
  bool do_hijing_split = !hijingfile.empty();
  if (do_hijing_split && gSystem->AccessPathName(hijingfile.c_str()))
  {
    std::cout << "HIJING tower tree " << hijingfile << " not found, no PYTHIA/HIJING split" << std::endl;
    do_hijing_split = false;
  }
  if (do_hijing_split)
  {
    auto *htl = new HijingTowerLoader(hijingfile);
    htl->set_scale_factor(hijing_scale);
    htl->set_input_tower_prefix(tower_prefix);
    htl->set_output_tower_prefix(hijing_prefix);
    se->registerSubsystem(htl);

    se->registerSubsystem(Retower("RetowerCEMC_HIJING", hijing_prefix));
  }

  // unsubtracted jets
  auto *tjr = new JetReco("TowerJetReco");
  for (const auto &src : {Jet::CEMC_TOWERINFO_RETOWER, Jet::HCALIN_TOWERINFO, Jet::HCALOUT_TOWERINFO})
  {
    tjr->add_input(TowerInput(src, tower_prefix));
  }
  tjr->add_algo(HIJETS::GetFJAlgo(jet_R), "AntiKt_TowerInfo_r03");
  tjr->set_algo_node("ANTIKT");
  tjr->set_input_node("TOWER");
  se->registerSubsystem(tjr);

  // one multiplicity rho per calorimeter layer
  const std::vector<std::pair<Jet::SRC, std::string>> layers = {
      {Jet::CEMC_TOWERINFO_RETOWER, "CEMC"},
      {Jet::HCALIN_TOWERINFO, "HCALIN"},
      {Jet::HCALOUT_TOWERINFO, "HCALOUT"}};
  std::vector<std::string> rho_nodes;
  for (const auto &[src, layer] : layers)
  {
    rho_nodes.push_back("TowerRho_" + layer + "_MULT");
    auto *rho = new DetermineTowerRho("DetermineTowerRho_" + layer);
    rho->add_method(TowerRho::Method::MULT, rho_nodes.back());
    rho->add_input(TowerInput(src, tower_prefix));
    rho->set_tower_shape_path(rho_smap_payload);
    rho->set_vertex_type(GlobalVertex::MBD);
    se->registerSubsystem(rho);
  }

  // rho -> UE per eta ring (affine in rho, interpolated in z vertex and MBD charge)
  auto *rho2bkg = new TowerRhoToBackground();
  rho2bkg->set_emcal_rho_node(rho_nodes[0]);
  rho2bkg->set_ihcal_rho_node(rho_nodes[1]);
  rho2bkg->set_ohcal_rho_node(rho_nodes[2]);
  rho2bkg->set_background_node("TowerInfoBackground_RhoSub");
  rho2bkg->set_vertex_type(GlobalVertex::MBD);
  rho2bkg->set_eta_calib_path(rho_ue_payload);
  rho2bkg->set_calib_model(TowerRhoToBackground::CalibModel::AFFINE);
  se->registerSubsystem(rho2bkg);

  // TOWERINFO_RHOSUB_{CEMC_RETOWER,HCALIN,HCALOUT}
  auto *st = new SubtractTowers("SubtractTowers_RhoSub");
  st->set_towerNodePrefix(tower_prefix);
  st->set_inputTowerBackgroundNode("TowerInfoBackground_RhoSub");
  st->set_emcal_output_node(rho_sub_prefix + "_CEMC_RETOWER");
  st->set_ihcal_output_node(rho_sub_prefix + "_HCALIN");
  st->set_ohcal_output_node(rho_sub_prefix + "_HCALOUT");
  st->set_ue_live_fraction(rho_ue_live_fraction);
  se->registerSubsystem(st);

  // the subtracted towers keep the unsubtracted source ids under their own prefix
  tjr = new JetReco("TowerJetReco_Rho");
  for (const auto &src : {Jet::CEMC_TOWERINFO_RETOWER, Jet::HCALIN_TOWERINFO, Jet::HCALOUT_TOWERINFO})
  {
    tjr->add_input(TowerInput(src, rho_sub_prefix));
  }
  tjr->add_algo(HIJETS::GetFJAlgo(jet_R), "AntiKt_TowerInfo_r03_Rho");
  tjr->set_algo_node("ANTIKT");
  tjr->set_input_node("TOWER");
  se->registerSubsystem(tjr);

  // the tree
  auto *anaout = new AnaTreev1(outfile);
  anaout->add_event_header("EventHeader");
  anaout->add_zvrtx_node("GlobalVertexMap");
  anaout->add_mbd_node("MbdOut");
  anaout->add_minbias_node("MinimumBiasInfo");
  anaout->add_cent_node("CentralityInfo");
  if (is_jet_sample)
  {
    anaout->add_phHep_node("PHHepMCGenEventMap");
    anaout->add_truth_jet_node("AntiKt_Truth_r03");
  }
  anaout->add_jet_node("AntiKt_TowerInfo_r03");
  anaout->add_rho_jet_node("AntiKt_TowerInfo_r03_Rho", rho_nodes[0], rho_nodes[1], rho_nodes[2], rho_sub_prefix);
  for (const auto &rho_node : rho_nodes)
  {
    anaout->add_rho_nodes(rho_node);
  }
  if (do_hijing_split)
  {
    anaout->add_hijing_tower_prefix(hijing_prefix);
  }
  // true: also save the full 24 x 64 tower grids
  anaout->add_cemc_node(tower_prefix + "_CEMC_RETOWER", true);
  anaout->add_ihcal_node(tower_prefix + "_HCALIN", true);
  anaout->add_ohcal_node(tower_prefix + "_HCALOUT", true);
  se->registerSubsystem(anaout);

  se->run(nEvents);
  se->End();
  se->PrintTimer();
  delete se;

  std::cout << "Done" << std::endl;
  gSystem->Exit(0);
}

#endif
