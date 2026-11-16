#include "HijingTowerLoader.h"

#include <fun4all/Fun4AllReturnCodes.h>

#include <ffaobjects/EventHeader.h>

#include <calobase/TowerInfo.h>
#include <calobase/TowerInfoContainer.h>

#include <phool/PHCompositeNode.h>
#include <phool/PHIODataNode.h>
#include <phool/PHNodeIterator.h>
#include <phool/getClass.h>
#include <phool/phool.h>

#include <TFile.h>
#include <TTree.h>

#include <iostream>
#include <limits>

namespace
{
  // node layer name and pass-1 (CaloTree) branch stem, indexed like the members
  const char *const k_layer_name[3] = {"CEMC", "HCALIN", "HCALOUT"};
  const char *const k_tree_stem[3] = {"cemc", "hcalin", "hcalout"};
  const char *const k_tree_count[3] = {"ncemc", "nhcalin", "nhcalout"};
}  // namespace

HijingTowerLoader::HijingTowerLoader(const std::string &hijing_tree_file, const std::string &name)
  : SubsysReco(name)
  , m_tree_file(hijing_tree_file)
{
}

int HijingTowerLoader::Init(PHCompositeNode * /*topNode*/)
{
  m_tfile = TFile::Open(m_tree_file.c_str(), "READ");
  if (!m_tfile || m_tfile->IsZombie())
  {
    std::cout << PHWHERE << " cannot open HIJING-only tower tree " << m_tree_file << ", abort" << std::endl;
    return Fun4AllReturnCodes::ABORTRUN;
  }
  m_ttree = dynamic_cast<TTree *>(m_tfile->Get("T"));
  if (!m_ttree)
  {
    std::cout << PHWHERE << " no tree T in " << m_tree_file << ", abort" << std::endl;
    return Fun4AllReturnCodes::ABORTRUN;
  }

  // index the entries by evtsequence, reading that branch alone
  m_ttree->SetBranchStatus("*", false);
  m_ttree->SetBranchStatus("evtsequence", true);
  m_ttree->SetBranchAddress("evtsequence", &m_tree_evtsequence);
  const int64_t nentries = m_ttree->GetEntries();
  for (int64_t i = 0; i < nentries; ++i)
  {
    m_ttree->GetEntry(i);
    if (!m_entry_of_evtsequence.emplace(m_tree_evtsequence, i).second)
    {
      std::cout << PHWHERE << " evtsequence " << m_tree_evtsequence << " appears twice in " << m_tree_file
                << ", keeping the first entry" << std::endl;
    }
  }

  m_ttree->SetBranchStatus("runnumber", true);
  m_ttree->SetBranchStatus("b", true);
  m_ttree->SetBranchStatus("ep_angle", true);
  m_ttree->SetBranchStatus("ecc", true);
  m_ttree->SetBranchStatus("ncoll", true);
  m_ttree->SetBranchStatus("psi1", true);
  m_ttree->SetBranchStatus("psi2", true);
  m_ttree->SetBranchStatus("psi3", true);
  m_ttree->SetBranchAddress("runnumber", &m_tree_runnumber);
  m_ttree->SetBranchAddress("b", &m_tree_b);
  m_ttree->SetBranchAddress("ep_angle", &m_tree_ep_angle);
  m_ttree->SetBranchAddress("ecc", &m_tree_ecc);
  m_ttree->SetBranchAddress("ncoll", &m_tree_ncoll);
  m_ttree->SetBranchAddress("psi1", &m_tree_psi1);
  m_ttree->SetBranchAddress("psi2", &m_tree_psi2);
  m_ttree->SetBranchAddress("psi3", &m_tree_psi3);
  for (int il = 0; il < k_nlayers; ++il)
  {
    const std::string energy = std::string(k_tree_stem[il]) + "_tower_energy";
    const std::string status = std::string(k_tree_stem[il]) + "_tower_status";
    m_ttree->SetBranchStatus(k_tree_count[il], true);
    m_ttree->SetBranchStatus(energy.c_str(), true);
    m_ttree->SetBranchStatus(status.c_str(), true);
    m_ttree->SetBranchAddress(k_tree_count[il], &m_tree_nchannels[il]);
    m_ttree->SetBranchAddress(energy.c_str(), &m_tree_energy[il]);
    m_ttree->SetBranchAddress(status.c_str(), &m_tree_status[il]);
  }

  std::cout << "HijingTowerLoader::Init - " << m_tree_file << ": " << nentries << " entries, "
            << m_entry_of_evtsequence.size() << " distinct evtsequence; scale factor " << m_scale_factor
            << ", " << m_in_prefix << "_* -> " << m_out_prefix << "_*" << std::endl;

  return Fun4AllReturnCodes::EVENT_OK;
}

int HijingTowerLoader::InitRun(PHCompositeNode *topNode)
{
  PHNodeIterator iter(topNode);
  auto *dstNode = dynamic_cast<PHCompositeNode *>(iter.findFirst("PHCompositeNode", "DST"));
  if (!dstNode)
  {
    std::cout << PHWHERE << " DST node missing, abort" << std::endl;
    return Fun4AllReturnCodes::ABORTRUN;
  }

  for (int il = 0; il < k_nlayers; ++il)
  {
    const std::string in_name = m_in_prefix + "_" + k_layer_name[il];
    const std::string out_name = m_out_prefix + "_" + k_layer_name[il];

    m_in_towers[il] = findNode::getClass<TowerInfoContainer>(topNode, in_name);
    if (!m_in_towers[il])
    {
      std::cout << PHWHERE << " input node " << in_name << " missing, abort" << std::endl;
      return Fun4AllReturnCodes::ABORTRUN;
    }

    m_out_towers[il] = findNode::getClass<TowerInfoContainer>(topNode, out_name);
    if (!m_out_towers[il])
    {
      // a copy of the input keeps its channel layout and tower class
      m_out_towers[il] = dynamic_cast<TowerInfoContainer *>(m_in_towers[il]->CloneMe());

      PHNodeIterator dstiter(dstNode);
      auto *layerNode = dynamic_cast<PHCompositeNode *>(dstiter.findFirst("PHCompositeNode", k_layer_name[il]));
      if (!layerNode)
      {
        layerNode = new PHCompositeNode(k_layer_name[il]);
        dstNode->addNode(layerNode);
      }
      layerNode->addNode(new PHIODataNode<PHObject>(m_out_towers[il], out_name, "PHObject"));
    }
  }

  return Fun4AllReturnCodes::EVENT_OK;
}

int HijingTowerLoader::process_event(PHCompositeNode *topNode)
{
  ++m_nevents;

  int64_t entry = -1;
  const int found = FindEntry(topNode, entry);
  if (found == Fun4AllReturnCodes::ABORTRUN)
  {
    return found;
  }
  if (entry < 0)
  {
    ++m_nunmatched;
    FillNaN();
    return Fun4AllReturnCodes::EVENT_OK;
  }

  for (int il = 0; il < k_nlayers; ++il)
  {
    auto *in = m_in_towers[il];
    auto *out = m_out_towers[il];
    const auto &tree_energy = *m_tree_energy[il];
    const auto &tree_status = *m_tree_status[il];

    // pass 1 stored the same node type channel by channel; a different count
    // means a different grid (e.g. retowered vs native CEMC), which cannot be
    // matched
    const int nch = static_cast<int>(in->size());
    if (m_tree_nchannels[il] != nch || static_cast<int>(tree_energy.size()) != nch || static_cast<int>(tree_status.size()) != nch)
    {
      std::cout << PHWHERE << " " << k_layer_name[il] << " has " << nch << " channels but the HIJING tree has "
                << m_tree_nchannels[il] << " (" << tree_energy.size() << " energies), abort" << std::endl;
      return Fun4AllReturnCodes::ABORTRUN;
    }

    for (int ich = 0; ich < nch; ++ich)
    {
      auto *tower = in->get_tower_at_channel(ich);
      auto *hijing = out->get_tower_at_channel(ich);
      if (!tower || !hijing)
      {
        continue;
      }
      hijing->copy_tower(tower);

      // CaloManip scaled only towers good in both passes; elsewhere the DST
      // still holds the unscaled HIJING energy
      const bool scaled = (tree_status[ich] == 1) && tower->get_isGood();
      if (!scaled && tower->get_isGood() && m_scale_factor != 1.0F)
      {
        ++m_nchannels_unscaled;
      }
      hijing->set_energy(tree_energy[ich] * (scaled ? m_scale_factor : 1.0F));
    }
  }

  return Fun4AllReturnCodes::EVENT_OK;
}

int HijingTowerLoader::End(PHCompositeNode * /*topNode*/)
{
  std::cout << "HijingTowerLoader::End - " << m_nevents << " events, " << m_nunmatched
            << " without a matching HIJING entry (towers set to NaN)";
  if (m_scale_factor != 1.0F)
  {
    std::cout << ", " << m_nchannels_unscaled << " good tower-events taken unscaled (bad in pass 1)";
  }
  std::cout << std::endl;

  if (m_tfile)
  {
    m_tfile->Close();
    delete m_tfile;
    m_tfile = nullptr;
    m_ttree = nullptr;
  }
  return Fun4AllReturnCodes::EVENT_OK;
}

int HijingTowerLoader::FindEntry(PHCompositeNode *topNode, int64_t &entry)
{
  entry = -1;

  auto *eventhead = findNode::getClass<EventHeader>(topNode, m_eventhead_node);
  if (!eventhead)
  {
    std::cout << PHWHERE << " " << m_eventhead_node << " node missing, abort" << std::endl;
    return Fun4AllReturnCodes::ABORTRUN;
  }

  const auto it = m_entry_of_evtsequence.find(eventhead->get_EvtSequence());
  if (it == m_entry_of_evtsequence.end())
  {
    if (m_nunmatched < 5)
    {
      std::cout << PHWHERE << " evtsequence " << eventhead->get_EvtSequence() << " not in " << m_tree_file << std::endl;
    }
    return Fun4AllReturnCodes::EVENT_OK;
  }
  m_ttree->GetEntry(it->second);

  // same generator-level cross-check as CaloManip, which made the DST from this tree
  if (eventhead->get_RunNumber() != m_tree_runnumber
      || eventhead->get_ImpactParameter() != m_tree_b
      || eventhead->get_EventPlaneAngle() != m_tree_ep_angle
      || eventhead->get_eccentricity() != m_tree_ecc
      || static_cast<float>(eventhead->get_ncoll()) != m_tree_ncoll
      || eventhead->get_FlowPsiN(1) != m_tree_psi1
      || eventhead->get_FlowPsiN(2) != m_tree_psi2
      || eventhead->get_FlowPsiN(3) != m_tree_psi3)
  {
    if (m_nunmatched < 5)
    {
      std::cout << PHWHERE << " evtsequence " << eventhead->get_EvtSequence()
                << " found but its HIJING observables differ (b " << eventhead->get_ImpactParameter()
                << " vs " << m_tree_b << ", psi2 " << eventhead->get_FlowPsiN(2) << " vs " << m_tree_psi2
                << ")" << std::endl;
    }
    return Fun4AllReturnCodes::EVENT_OK;
  }

  entry = it->second;
  return Fun4AllReturnCodes::EVENT_OK;
}

void HijingTowerLoader::FillNaN()
{
  for (int il = 0; il < k_nlayers; ++il)
  {
    const int nch = static_cast<int>(m_in_towers[il]->size());
    for (int ich = 0; ich < nch; ++ich)
    {
      auto *tower = m_in_towers[il]->get_tower_at_channel(ich);
      auto *hijing = m_out_towers[il]->get_tower_at_channel(ich);
      if (!tower || !hijing)
      {
        continue;
      }
      hijing->copy_tower(tower);
      hijing->set_energy(std::numeric_limits<float>::quiet_NaN());
    }
  }
}
