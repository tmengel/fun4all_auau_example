# fun4all_auau_example

A Fun4All macro and a small analysis library that turn an embedded Au+Au
simulation DST (PYTHIA jets in HIJING) into a flat analysis `TTree` with
rho-subtracted R = 0.3 calorimeter jets.

| path | content |
| --- | --- |
| `macro/Fun4All_Dijets_Sim.C` | the macro |
| `src/myana` | `libmyana`: the tree writer and the event selection, the only code needed besides coresoftware |
| `setup_rho.sh` | environment setup (SDCC only), see below |

The rho-based underlying-event subtraction is coresoftware PR #4465, which is
not in a release yet. Until it is, `setup_rho.sh` adds a build that has it.

## Quick start

Works on the SDCC machines only.

```bash
git clone https://github.com/tmengel/fun4all_auau_example.git
cd fun4all_auau_example

# 1. environment (every new shell)
source setup_rho.sh

# 2. build libmyana (once, and after every change in src/myana)
mkdir -p src/myana/build && cd src/myana/build
../autogen.sh --prefix=$MYINSTALL
make -j4 install
cd ../../..

# 3. make a test tree from 100 events
cd macro
root -l -q -b 'Fun4All_Dijets_Sim.C(100)'
```

Step 3 writes `sim_output_ana.root` with the tree `T`. Of the 100 events 91
pass the minimum bias selection. Look at it with

```bash
root -l sim_output_ana.root -e 'T->Scan("cent:rho_jet_pT:rho_jet_unsub_pT","rho_jet_pT>20")'
```

Build `libmyana` before the first run. Without your own build the macro still
runs, but with the `libmyana` of the shared install area, so changes you make in
`src/myana` have no effect.

## What `setup_rho.sh` does

`source setup_rho.sh [install dir]` (default install dir: `install/` in your clone)

- sets up the fixed `ana.585` build, the one the PR libraries were built against,
- exports `MYINSTALL`, the directory `libmyana` is installed to,
- puts `$MYINSTALL` and `/sphenix/user/tmengel/fun4all_auau_example/install` in
  front of the release. The second one holds `jetbase` and `jetbackground` built
  with PR #4465.

Source it in every new shell and at the top of condor job scripts.

Once the PR is in the `new` build, `setup_rho.sh` is no longer needed. Use the
standard setup instead, followed by the same build commands:

```bash
source /opt/sphenix/core/bin/sphenix_setup.sh -n new
export MYINSTALL=/path/to/install
source $OPT_SPHENIX/bin/setup_local.sh $MYINSTALL
```

## Running the macro

```bash
cd macro
root -l -q -b 'Fun4All_Dijets_Sim.C(100, "DST_in.root", "tree_out.root")'
```

Arguments, in order:

| argument | default | meaning |
| --- | --- | --- |
| `nEvents` | 10 | events to process, 0 or negative = all |
| `infile` | jet30 segment 100 | input DST with calibrated towers, MBD, global vertex, truth jets and HepMC |
| `outfile` | `sim_output_ana.root` | output file with the tree `T` |
| `is_jet_sample` | true | save truth jets and the hard-scatter partons; false for minimum bias HIJING |
| `hijingfile` | `""` | tree of HIJING-only towers of the same segment, switches on the PYTHIA/HIJING split |
| `hijing_scale` | 1.11 | scale applied to the HIJING towers in the DST (1 for the unscaled sample) |
| `rho_ue_payload` | scaled HIJING payload | `TowerRhoToBackground` calibration: UE per layer and eta ring as a function of rho |
| `rho_smap_payload` | scaled HIJING payload | `DetermineTowerRho` tower shape map |
| `rho_ue_live_fraction` | true | subtract only the UE of the live area of each retowered EMCal tower |

## What the macro does

1. `RetowerCEMC` puts the EMCal on the HCal tower grid.
2. `MinimumBiasClassifier` and `CentralityReco` give the minimum bias decision and centrality.
3. `EventSelector` with a `MinBiasCut` drops events that are not minimum bias.
4. Optional: `HijingTowerLoader` and a second `RetowerCEMC` put the HIJING-only towers on the node tree.
5. `JetReco` makes unsubtracted R = 0.3 tower jets.
6. `DetermineTowerRho`, once per calorimeter layer, measures the multiplicity rho.
7. `TowerRhoToBackground` converts the three rho values into the UE per layer and eta ring.
8. `SubtractTowers` writes the subtracted towers `TOWERINFO_RHOSUB_*`.
9. `JetReco` makes the R = 0.3 jets `AntiKt_TowerInfo_r03_Rho` from the subtracted towers.
10. `AnaTreev1` writes the tree.

## Remaking the trees with the PYTHIA/HIJING split

The HIJING-only tower trees exist only for the DSTs listed here.
The branches `rho_jet_pythia_E` and `rho_jet_hijing_E` are filled only when
`hijingfile` is given. They split the unsubtracted energy of every jet into the
part from HIJING and the rest (PYTHIA plus noise):
`rho_jet_pythia_E + rho_jet_hijing_E = rho_jet_unsub_E`.

Inputs, one file per segment `<seg>` (5 digits):

| input | path |
| --- | --- |
| embedded DST | `/sphenix/tg/tg01/jets/tmengel/ppg14/sim_scaling/08_14_2026_v001/dsts/jet30_hijing/scaled/DST_SCALED_jet30_hijing31_pass2-<seg>.root` |
| HIJING-only towers | `/sphenix/tg/tg01/jets/tmengel/ppg14/sim_scaling/08_14_2026_v001/trees/CALO_TREE_noNoise_hijing31_pass1-<seg>.root` |

The HIJING file must be the same segment as the DST. Events are matched by
`evtsequence`, so the two files do not need the same number of entries.

One segment, all events:

```bash
BASE=/sphenix/tg/tg01/jets/tmengel/ppg14/sim_scaling/08_14_2026_v001
SEG=00100
root -l -q -b "Fun4All_Dijets_Sim.C(0, \
  \"$BASE/dsts/jet30_hijing/scaled/DST_SCALED_jet30_hijing31_pass2-$SEG.root\", \
  \"CALO_TREE_SCALED_jet30_hijing31_pass3-$SEG.root\", \
  true, \
  \"$BASE/trees/CALO_TREE_noNoise_hijing31_pass1-$SEG.root\")"
```

All segments of a sample:

```bash
BASE=/sphenix/tg/tg01/jets/tmengel/ppg14/sim_scaling/08_14_2026_v001
for DST in $BASE/dsts/jet30_hijing/scaled/DST_SCALED_jet30_hijing31_pass2-*.root; do
  SEG=${DST##*-}; SEG=${SEG%.root}
  HIJING=$BASE/trees/CALO_TREE_noNoise_hijing31_pass1-$SEG.root
  [ -f "$HIJING" ] || continue
  root -l -q -b "Fun4All_Dijets_Sim.C(0, \"$DST\", \"CALO_TREE_SCALED_jet30_hijing31_pass3-$SEG.root\", true, \"$HIJING\")"
done
```

A segment takes a few minutes, so run the full sample (about 15000 segments) as
one condor job per segment.

Other samples in the same directory tree:

| sample | DST directory and file prefix | `is_jet_sample` | `hijing_scale` |
| --- | --- | --- | --- |
| jet10, jet20 | `dsts/jet10_hijing/scaled/DST_SCALED_jet10_...`, same for jet20 | true | 1.11 |
| minimum bias, scaled | `dsts/mb_hijing/scaled/DST_SCALED_mb_...` | false | 1.11 |
| minimum bias, unscaled | `dsts/mb_hijing/unsclaed/DST_UNSCALED_mb_...` | false | 1.0 |

The rho payload arguments default to the calibration of the UE-scaled sample,
`/sphenix/user/tmengel/jstg03/calibrations/rho_ue_calib_prod_v1/mb_hijing31_scaled_v1`.
The unscaled sample needs `rho_ue_payload.root` and `smap_payload.root` from
`mb_hijing31_std_v1` in the same directory, passed as arguments 7 and 8.

Quick check of the split:

```bash
root -l tree_out.root -e 'T->Scan("rho_jet_pT:rho_jet_unsub_E:rho_jet_pythia_E:rho_jet_hijing_E","rho_jet_pT>20")'
```

If `hijingfile` does not exist the macro prints a message and runs without the
split. An event with no match in the HIJING file gets NaN in the split branches.

## Modules in `src/myana`

| class | role |
| --- | --- |
| `AnaTreev1` | writes the tree; every `add_*_node` call switches on one group of branches |
| `EventSelector` | aborts events that fail any of its `EventCut`s |
| `MinBiasCut` | an `EventCut` on `MinimumBiasInfo` |
| `EventCut`, `EventCutReport` | cut base class and the cut-flow printout |
| `HijingTowerLoader` | reads the HIJING-only tower energies of the current event from a tree and puts them on the node tree as `TOWERINFO_HIJING_*` |

## Tree content

One entry per selected event.

| branches | content |
| --- | --- |
| `event_id`, `runnumber`, `evtsequence` | event bookkeeping |
| `is_minbias`, `cent`, `zvrtx`, `mbd_*` | minimum bias flag, centrality, vertex, MBD charge and time |
| `b`, `ncoll`, `npart`, `ep_angle`, `ecc`, `psi1..3` | HIJING event header |
| `truth_jet_*`, `truth_zvtx` | truth jets with flavor and matched parton (jet samples) |
| `sumeT_*`, `*_tower_E`, `*_tower_isgood` | per-layer transverse energy sum and the 24 x 64 tower grids |
| `jet_*` | unsubtracted jets |
| `rho_jet_E`, `_pT`, `_eta`, `_phi` | rho-subtracted jets |
| `rho_jet_unsub_E`, `_unsub_pT`, `rho_jet_constituent_*` | the same jets before subtraction, and their towers |
| `rho_jet_pythia_E`, `rho_jet_hijing_E`, `rho_jet_{pythia,hijing}_{emfrac,ihcalfrac,ohcalfrac}` | PYTHIA/HIJING split and its layer fractions, only with `hijingfile` |
| `rho_val_TowerRho_<layer>_MULT`, `rho_sigma_TowerRho_<layer>_MULT` | rho and its width per calorimeter layer |
