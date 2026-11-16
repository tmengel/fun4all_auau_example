#ifndef MYANA_HIJINGTOWERLOADER_H
#define MYANA_HIJINGTOWERLOADER_H

#include <fun4all/SubsysReco.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

class PHCompositeNode;
class TowerInfoContainer;
class TFile;
class TTree;

/// \class HijingTowerLoader
///
/// \brief Puts the HIJING-only tower energies of the current embedded event on
/// the node tree, so jets can be split into their PYTHIA and HIJING parts.
///
/// The UE-scaled simulation (macros/hijing/Fun4All_UEScaling_Pass{1,2}.C) is
///   pass 1: HIJING-only G4 hits -> noiseless waveforms -> towers, written per
///           channel by CaloTree to CALO_TREE_noNoise_hijing31_pass1-<seg>.root
///   pass 2: official PYTHIA+HIJING embedded towers, plus (scale - 1) x the pass-1
///           towers (CaloManip) -> DST_SCALED_*_pass2-<seg>.root
/// so the HIJING content of a tower in the pass-2 DST is
///   E_hijing = E_pass1 x scale   (scale = 1 where CaloManip skipped the tower)
/// and E_total - E_hijing is the PYTHIA signal plus electronic noise.
///
/// For every input layer <in_prefix>_<layer> this writes <out_prefix>_<layer>,
/// a copy of the input container (same channels and tower status) holding
/// E_hijing. Run RetowerCEMC with set_towerNodePrefix( out_prefix ) afterwards
/// to get <out_prefix>_CEMC_RETOWER on the same grid as the jet inputs.
///
/// The pass-1 entry is found by the EventHeader evtsequence, not by entry
/// number (pass 2 and the event selection both drop events), and the HIJING
/// generator observables are cross-checked as in CaloManip. An event with no
/// matching entry gets NaN energies, so nothing derived from it passes as valid.
class HijingTowerLoader : public SubsysReco
{
 public:
  HijingTowerLoader(const std::string &hijing_tree_file, const std::string &name = "HijingTowerLoader");
  ~HijingTowerLoader() override = default;

  int Init(PHCompositeNode *topNode) override;
  int InitRun(PHCompositeNode *topNode) override;
  int process_event(PHCompositeNode *topNode) override;
  int End(PHCompositeNode *topNode) override;

  /// UE scale CaloManip applied in pass 2: 1.11 for DST_SCALED, 1 for DST_UNSCALED
  void set_scale_factor(const float s) { m_scale_factor = s; }
  void set_input_tower_prefix(const std::string &p) { m_in_prefix = p; }
  void set_output_tower_prefix(const std::string &p) { m_out_prefix = p; }
  void set_event_header(const std::string &n) { m_eventhead_node = n; }

 private:
  // pass-1 branch names and node layers, indexed 0 = CEMC, 1 = HCALIN, 2 = HCALOUT
  static constexpr int k_nlayers = 3;

  std::string m_tree_file{""};
  std::string m_eventhead_node{"EventHeader"};
  std::string m_in_prefix{"TOWERINFO_CALIB"};
  std::string m_out_prefix{"TOWERINFO_HIJING"};
  float m_scale_factor{1.0};

  TFile *m_tfile{nullptr};
  TTree *m_ttree{nullptr};
  std::map<int, int64_t> m_entry_of_evtsequence{};

  int m_tree_runnumber{-1};
  int m_tree_evtsequence{-1};
  float m_tree_b{0.0};
  float m_tree_ep_angle{0.0};
  float m_tree_ecc{0.0};
  float m_tree_ncoll{0.0};
  float m_tree_psi1{0.0};
  float m_tree_psi2{0.0};
  float m_tree_psi3{0.0};
  int m_tree_nchannels[k_nlayers]{-1, -1, -1};
  std::vector<float> *m_tree_energy[k_nlayers]{nullptr, nullptr, nullptr};
  std::vector<int> *m_tree_status[k_nlayers]{nullptr, nullptr, nullptr};

  TowerInfoContainer *m_in_towers[k_nlayers]{nullptr, nullptr, nullptr};
  TowerInfoContainer *m_out_towers[k_nlayers]{nullptr, nullptr, nullptr};

  long m_nevents{0};
  long m_nunmatched{0};
  long m_nchannels_unscaled{0};

  int FindEntry(PHCompositeNode *topNode, int64_t &entry);
  void FillNaN();
};

#endif  // MYANA_HIJINGTOWERLOADER_H
