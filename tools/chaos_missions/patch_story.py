"""Build a local copy of main.scm with selected mission changes."""

import argparse
from hashlib import sha256
from pathlib import Path
from struct import pack, unpack_from

import against_all_odds
import back_to_school
import beat_down_on_b_dup
import big_smoke
import body_harvest
import cesar_vialpando
import customs_fast_track
import cut_throat_business
import deconstruction
import doberman
import don_peyote
import farewell_my_love
import fish_in_a_barrel
import going_to_san_fierro
import grove_4_life
import high_noon
import in_the_beginning
import home_coming
import jizzy
import key_to_her_heart
import king_in_exile
import learning_to_fly
import madd_dogg
import mike_toreno
import mountain_cloud_boys
import new_model_army
import photo_opportunity
import pier_69
import puncture_wounds
import riot
import saint_marks_bistro
import t_bone_mendez
import up_up_and_away
import verdant_meadows
import wear_flowers
import wu_zi_mu
import yay_ka_boom_boom
import lifes_a_beach
import management_issues
import we_tip
import noe
import stowaway
import local_liquor_store
import small_town_bank
import torenos_last_flight
import test_drive
import air_raid
import amphibious_assault
import architectural_espionage
import da_nang_thang
import dam_and_blast
import drive_by
import green_goo
import home_in_the_hills
import house_party
import interdiction
import los_sepulcros
import madd_doggs_rhymes
import misappropriation
import monster
import og_loc
import reuniting_the_families
import snail_trail
import vertical_bird
import badlands
import black_project
import breaking_the_bank
import burning_desire
import catalyst
import cleaning_the_hood
import cop_wheels
import drive_thru
import end_of_the_line
import explosive_situation
import fender_ketchup
import freefall
import gray_imports
import highjack
import high_stakes_low_rider
import home_invasion
import ice_cold_killa
import intensive_care
import just_business
import los_desperados
import lure
import meat_business
import nines_and_aks
import outrider
import ran_fa_li
import robbing_uncle_sam
import running_dog
import ryder
import supply_lines
import sweet_and_kendl
import sweets_girl
import tanker_commander
import the_green_sabre
import tagging_up_turf
import wrong_side_of_the_tracks
import youve_had_your_chips
import zeroing_in
from scm import Campaign, Mission, OpcodeTable

BASELINE_SHA256 = "601def3baae766ce6a23e2f0b9b48f6b33c9a64e2fc32eb4f22ddea8b868b0fa"
MISSION_TABLE_OFFSET = 0xCFA4 + 24
MISSIONS = {
    "in-the-beginning": in_the_beginning,
    "sweet-and-kendl": sweet_and_kendl,
    "ryder": ryder,
    "tagging-up-turf": tagging_up_turf,
    "cleaning-the-hood": cleaning_the_hood,
    "drive-thru": drive_thru,
    "nines-and-aks": nines_and_aks,
    "sweets-girl": sweets_girl,
    "home-invasion": home_invasion,
    "catalyst": catalyst,
    "robbing-uncle-sam": robbing_uncle_sam,
    "running-dog": running_dog,
    "wrong-side-of-the-tracks": wrong_side_of_the_tracks,
    "just-business": just_business,
    "burning-desire": burning_desire,
    "gray-imports": gray_imports,
    "high-stakes-low-rider": high_stakes_low_rider,
    "the-green-sabre": the_green_sabre,
    "badlands": badlands,
    "tanker-commander": tanker_commander,
    "outrider": outrider,
    "ice-cold-killa": ice_cold_killa,
    "ran-fa-li": ran_fa_li,
    "lure": lure,
    "zeroing-in": zeroing_in,
    "supply-lines": supply_lines,
    "highjack": highjack,
    "black-project": black_project,
    "fender-ketchup": fender_ketchup,
    "explosive-situation": explosive_situation,
    "youve-had-your-chips": youve_had_your_chips,
    "cop-wheels": cop_wheels,
    "intensive-care": intensive_care,
    "meat-business": meat_business,
    "freefall": freefall,
    "breaking-the-bank": breaking_the_bank,
    "los-desperados": los_desperados,
    "end-of-the-line": end_of_the_line,
    "air-raid": air_raid,
    "amphibious-assault": amphibious_assault,
    "architectural-espionage": architectural_espionage,
    "da-nang-thang": da_nang_thang,
    "dam-and-blast": dam_and_blast,
    "drive-by": drive_by,
    "green-goo": green_goo,
    "home-in-the-hills": home_in_the_hills,
    "house-party": house_party,
    "interdiction": interdiction,
    "los-sepulcros": los_sepulcros,
    "madd-doggs-rhymes": madd_doggs_rhymes,
    "misappropriation": misappropriation,
    "monster": monster,
    "og-loc": og_loc,
    "reuniting-the-families": reuniting_the_families,
    "snail-trail": snail_trail,
    "vertical-bird": vertical_bird,
    "lifes-a-beach": lifes_a_beach,
    "management-issues": management_issues,
    "we-tip": we_tip,
    "noe": noe,
    "stowaway": stowaway,
    "local-liquor-store": local_liquor_store,
    "small-town-bank": small_town_bank,
    "torenos-last-flight": torenos_last_flight,
    "test-drive": test_drive,
    "against-all-odds": against_all_odds,
    "back-to-school": back_to_school,
    "beat-down-on-b-dup": beat_down_on_b_dup,
    "big-smoke": big_smoke,
    "body-harvest": body_harvest,
    "cesar-vialpando": cesar_vialpando,
    "customs-fast-track": customs_fast_track,
    "cut-throat-business": cut_throat_business,
    "deconstruction": deconstruction,
    "doberman": doberman,
    "don-peyote": don_peyote,
    "farewell-my-love": farewell_my_love,
    "fish-in-a-barrel": fish_in_a_barrel,
    "going-to-san-fierro": going_to_san_fierro,
    "grove-4-life": grove_4_life,
    "high-noon": high_noon,
    "home-coming": home_coming,
    "jizzy": jizzy,
    "key-to-her-heart": key_to_her_heart,
    "king-in-exile": king_in_exile,
    "learning-to-fly": learning_to_fly,
    "madd-dogg": madd_dogg,
    "mike-toreno": mike_toreno,
    "mountain-cloud-boys": mountain_cloud_boys,
    "new-model-army": new_model_army,
    "photo-opportunity": photo_opportunity,
    "pier-69": pier_69,
    "puncture-wounds": puncture_wounds,
    "riot": riot,
    "saint-marks-bistro": saint_marks_bistro,
    "t-bone-mendez": t_bone_mendez,
    "up-up-and-away": up_up_and_away,
    "verdant-meadows": verdant_meadows,
    "wear-flowers": wear_flowers,
    "wu-zi-mu": wu_zi_mu,
    "yay-ka-boom-boom": yay_ka_boom_boom,
}


def patch_mission(original, patched, mission):
    entry = MISSION_TABLE_OFFSET + mission.MISSION_ID * 4
    start, end = unpack_from("<II", original, entry)
    name = original[start + 3 : start + 11].rstrip(b"\0")
    if original[start : start + 3] != b"\xA4\x03\x09" or name != mission.SCRIPT_NAME:
        raise ValueError(f"Unexpected mission script for {mission.NAME}")

    for offset, instruction_prefix, old_value, new_value in mission.EDITS:
        prefix = bytes.fromhex(instruction_prefix)
        operand_format = {4: "<b", 6: "<f"}[prefix[-1]]
        old_bytes = pack(operand_format, old_value)
        new_bytes = pack(operand_format, new_value)
        operand_offset = offset + len(prefix)
        operand_end = operand_offset + len(old_bytes)
        if not start <= offset < operand_end <= end:
            raise ValueError(f"Edit at 0x{offset:X} is outside {mission.NAME}")
        if patched[offset:operand_end] != prefix + old_bytes:
            raise ValueError(f"Unexpected instruction at 0x{offset:X} in {mission.NAME}")
        patched[operand_offset:operand_end] = new_bytes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("baseline", type=Path, help="Original main.scm")
    parser.add_argument("output", type=Path, help="New file to create")
    parser.add_argument("--mission", choices=MISSIONS, action="append")
    parser.add_argument("--difficulty", choices=("zor", "acimasiz"), default="zor")
    parser.add_argument("--opcodes", type=Path, help="Installed San Andreas opcode definitions (sa.json)")
    args = parser.parse_args()

    original = args.baseline.read_bytes()
    if sha256(original).hexdigest() != BASELINE_SHA256:
        parser.error("The input does not match the supported original main.scm.")

    opcode_file = args.opcodes or args.baseline.resolve().parents[2] / "cleo/.config/sa.json"
    if not opcode_file.is_file():
        parser.error("Pass --opcodes with the path to the installed sa.json.")
    table = OpcodeTable(opcode_file)
    staged = bytearray(original)
    selected = args.mission or MISSIONS
    modules = [MISSIONS[key] for key in dict.fromkeys(selected)]
    for module in modules:
        if hasattr(module, "EDITS"):
            patch_mission(original, staged, module)
    campaign = Campaign(bytes(staged))
    missions = {}
    for module in modules:
        parts = (
            zip(module.MISSION_IDS, module.PARTS, strict=True)
            if hasattr(module, "PARTS")
            else ((module.MISSION_ID, module.expand),)
        )
        for index, expand in parts:
            if index not in missions:
                missions[index] = Mission(campaign.mission(index), table, args.difficulty)
            expand(missions[index])
        print(f"Built {module.NAME}")

    patched = campaign.build({index: mission.build() for index, mission in missions.items()})
    with args.output.open("xb") as output:
        output.write(patched)
    print(f"SHA-256: {sha256(patched).hexdigest()}")


if __name__ == "__main__":
    main()
