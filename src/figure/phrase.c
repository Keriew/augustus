#include "phrase.h"

#include "building/building.h"
#include "building/market.h"
#include "city/constants.h"
#include "city/culture.h"
#include "city/figures.h"
#include "city/gods.h"
#include "city/labor.h"
#include "city/population.h"
#include "city/resource.h"
#include "city/sentiment.h"
#include "core/calc.h"
#include "core/file.h"
#include "figure/trader.h"
#include "figuretype/trader.h"
#include "sound/speech.h"

#include <stdio.h>

#define SOUND_FILENAME_MAX 64

static const char FIGURE_SOUNDS[33][20][SOUND_FILENAME_MAX] = {
    { // 0
        "wavs/vigils_starv1.wav", "wavs/vigils_nojob1.wav", "wavs/vigils_needjob1.wav", "wavs/vigils_nofun1.wav",
        "wavs/vigils_relig1.wav", "wavs/vigils_great1.wav", "wavs/vigils_great2.wav", "wavs/vigils_exact1.wav",
        "wavs/vigils_exact2.wav", "wavs/vigils_exact3.wav", "wavs/vigils_exact4.wav", "wavs/vigils_exact5.wav",
        "wavs/vigils_exact6.wav", "wavs/vigils_exact7.wav", "wavs/vigils_exact8.wav", "wavs/vigils_exact9.wav",
        "wavs/vigils_exact10.wav", "wavs/vigils_free1.wav", "wavs/vigils_free2.wav", "wavs/vigils_free3.wav"
    },
    { // 1
        "wavs/wallguard_starv1.wav", "wavs/wallguard_nojob1.wav", "wavs/wallguard_needjob1.wav", "wavs/wallguard_nofun1.wav",
        "wavs/wallguard_relig1.wav", "wavs/wallguard_great1.wav", "wavs/wallguard_great2.wav", "wavs/wallguard_exact1.wav",
        "wavs/wallguard_exact2.wav", "wavs/wallguard_exact3.wav", "wavs/wallguard_exact4.wav", "wavs/wallguard_exact5.wav",
        "wavs/wallguard_exact6.wav", "wavs/wallguard_exact7.wav", "wavs/wallguard_exact8.wav", "wavs/wallguard_exact9.wav",
        "wavs/wallguard_exact0.wav", "wavs/wallguard_free1.wav", "wavs/wallguard_free2.wav", "wavs/wallguard_free3.wav"
    },
    { // 2
        "wavs/engine_starv1.wav", "wavs/engine_nojob1.wav", "wavs/engine_needjob1.wav", "wavs/engine_nofun1.wav",
        "wavs/engine_relig1.wav", "wavs/engine_great1.wav", "wavs/engine_great2.wav", "wavs/engine_exact1.wav",
        "wavs/engine_exact2.wav", "wavs/engine_exact3.wav", "wavs/engine_exact4.wav", "wavs/engine_exact5.wav",
        "wavs/engine_exact6.wav", "wavs/engine_exact7.wav", "wavs/engine_exact8.wav", "wavs/engine_exact9.wav",
        "wavs/engine_exact0.wav", "wavs/engine_free1.wav", "wavs/engine_free2.wav", "wavs/engine_free3.wav"
    },
    { // 3
        "wavs/taxman_starv1.wav", "wavs/taxman_nojob1.wav", "wavs/taxman_needjob1.wav", "wavs/taxman_nofun1.wav",
        "wavs/taxman_relig1.wav", "wavs/taxman_great1.wav", "wavs/taxman_great2.wav", "wavs/taxman_exact1.wav",
        "wavs/taxman_exact2.wav", "wavs/taxman_exact3.wav", "wavs/taxman_exact4.wav", "wavs/taxman_exact5.wav",
        "wavs/taxman_exact6.wav", "wavs/taxman_exact7.wav", "wavs/taxman_exact8.wav", "wavs/taxman_exact9.wav",
        "wavs/taxman_exact0.wav", "wavs/taxman_free1.wav", "wavs/taxman_free2.wav", "wavs/taxman_free3.wav"
    },
    { // 4
        "wavs/market_starv1.wav", "wavs/market_nojob1.wav", "wavs/market_needjob1.wav", "wavs/market_nofun1.wav",
        "wavs/market_relig1.wav", "wavs/market_great1.wav", "wavs/market_great2.wav", "wavs/market_exact2.wav",
        "wavs/market_exact1.wav", "wavs/market_exact3.wav", "wavs/market_exact4.wav", "wavs/market_exact5.wav",
        "wavs/market_exact6.wav", "wavs/market_exact7.wav", "wavs/market_exact8.wav", "wavs/market_exact9.wav",
        "wavs/market_exact0.wav", "wavs/market_free1.wav", "wavs/market_free2.wav", "wavs/market_free3.wav"
    },
    { // 5
        "wavs/crtpsh_starv1.wav", "wavs/crtpsh_nojob1.wav", "wavs/crtpsh_needjob1.wav", "wavs/crtpsh_nofun1.wav",
        "wavs/crtpsh_relig1.wav", "wavs/crtpsh_great1.wav", "wavs/crtpsh_great2.wav", "wavs/crtpsh_exact1.wav",
        "wavs/crtpsh_exact2.wav", "wavs/crtpsh_exact3.wav", "wavs/crtpsh_exact4.wav", "wavs/crtpsh_exact5.wav",
        "wavs/crtpsh_exact6.wav", "wavs/crtpsh_exact7.wav", "wavs/crtpsh_exact8.wav", "wavs/crtpsh_exact9.wav",
        "wavs/crtpsh_exact0.wav", "wavs/crtpsh_free1.wav", "wavs/crtpsh_free2.wav", "wavs/crtpsh_free3.wav"
    },
    { // 6
        "wavs/donkey_starv1.wav", "wavs/donkey_nojob1.wav", "wavs/donkey_needjob1.wav", "wavs/donkey_nofun1.wav",
        "wavs/donkey_relig1.wav", "wavs/donkey_great1.wav", "wavs/donkey_great2.wav", "wavs/donkey_exact1.wav",
        "wavs/donkey_exact2.wav", "wavs/donkey_exact3.wav", "wavs/donkey_exact4.wav", "wavs/donkey_exact5.wav",
        "wavs/donkey_exact6.wav", "wavs/donkey_exact7.wav", "wavs/donkey_exact8.wav", "wavs/donkey_exact9.wav",
        "wavs/donkey_exact0.wav", "wavs/donkey_free1.wav", "wavs/donkey_free2.wav", "wavs/donkey_free3.wav"
    },
    { // 7
        "wavs/boats_starv1.wav", "wavs/boats_nojob1.wav", "wavs/boats_needjob1.wav", "wavs/boats_nofun1.wav",
        "wavs/boats_relig1.wav", "wavs/boats_great1.wav", "wavs/boats_great2.wav", "wavs/boats_exact2.wav",
        "wavs/boats_exact1.wav", "wavs/boats_exact3.wav", "wavs/boats_exact4.wav", "wavs/boats_exact5.wav",
        "wavs/boats_exact6.wav", "wavs/boats_exact7.wav", "wavs/boats_exact8.wav", "wavs/boats_exact9.wav",
        "wavs/boats_exact0.wav", "wavs/boats_free1.wav", "wavs/boats_free2.wav", "wavs/boats_free3.wav"
    },
    { // 8
        "wavs/priest_starv1.wav", "wavs/priest_nojob1.wav", "wavs/priest_needjob1.wav", "wavs/priest_nofun1.wav",
        "wavs/priest_relig1.wav", "wavs/priest_great1.wav", "wavs/priest_great2.wav", "wavs/priest_exact1.wav",
        "wavs/priest_exact2.wav", "wavs/priest_exact3.wav", "wavs/priest_exact4.wav", "wavs/priest_exact5.wav",
        "wavs/priest_exact6.wav", "wavs/priest_exact7.wav", "wavs/priest_exact8.wav", "wavs/priest_exact9.wav",
        "wavs/priest_exact0.wav", "wavs/priest_free1.wav", "wavs/priest_free2.wav", "wavs/priest_free3.wav"
    },
    { // 9
        "wavs/teach_starv1.wav", "wavs/teach_nojob1.wav", "wavs/teach_needjob1.wav", "wavs/teach_nofun1.wav",
        "wavs/teach_relig1.wav", "wavs/teach_great1.wav", "wavs/teach_great2.wav", "wavs/teach_exact1.wav",
        "wavs/teach_exact2.wav", "wavs/teach_exact3.wav", "wavs/teach_exact4.wav", "wavs/teach_exact5.wav",
        "wavs/teach_exact6.wav", "wavs/teach_exact7.wav", "wavs/teach_exact8.wav", "wavs/teach_exact9.wav",
        "wavs/teach_exact0.wav", "wavs/teach_free1.wav", "wavs/teach_free2.wav", "wavs/teach_free3.wav"
    },
    { // 10
        "wavs/pupils_starv1.wav", "wavs/pupils_nojob1.wav", "wavs/pupils_needjob1.wav", "wavs/pupils_nofun1.wav",
        "wavs/pupils_relig1.wav", "wavs/pupils_great1.wav", "wavs/pupils_great2.wav", "wavs/pupils_exact1.wav",
        "wavs/pupils_exact2.wav", "wavs/pupils_exact3.wav", "wavs/pupils_exact4.wav", "wavs/pupils_exact5.wav",
        "wavs/pupils_exact6.wav", "wavs/pupils_exact7.wav", "wavs/pupils_exact8.wav", "wavs/pupils_exact9.wav",
        "wavs/pupils_exact0.wav", "wavs/pupils_free1.wav", "wavs/pupils_free2.wav", "wavs/pupils_free3.wav"
    },
    { // 11
        "wavs/bather_starv1.wav", "wavs/bather_nojob1.wav", "wavs/bather_needjob1.wav", "wavs/bather_nofun1.wav",
        "wavs/bather_relig1.wav", "wavs/bather_great1.wav", "wavs/bather_great2.wav", "wavs/bather_exact1.wav",
        "wavs/bather_exact2.wav", "wavs/bather_exact3.wav", "wavs/bather_exact4.wav", "wavs/bather_exact5.wav",
        "wavs/bather_exact6.wav", "wavs/bather_exact7.wav", "wavs/bather_exact8.wav", "wavs/bather_exact9.wav",
        "wavs/bather_exact0.wav", "wavs/bather_free1.wav", "wavs/bather_free2.wav", "wavs/bather_free3.wav"
    },
    { // 12
        "wavs/doctor_starv1.wav", "wavs/doctor_nojob1.wav", "wavs/doctor_needjob1.wav", "wavs/doctor_nofun1.wav",
        "wavs/doctor_relig1.wav", "wavs/doctor_great1.wav", "wavs/doctor_great2.wav", "wavs/doctor_exact1.wav",
        "wavs/doctor_exact2.wav", "wavs/doctor_exact3.wav", "wavs/doctor_exact4.wav", "wavs/doctor_exact5.wav",
        "wavs/doctor_exact6.wav", "wavs/doctor_exact7.wav", "wavs/doctor_exact8.wav", "wavs/doctor_exact9.wav",
        "wavs/doctor_exact0.wav", "wavs/doctor_free1.wav", "wavs/doctor_free2.wav", "wavs/doctor_free3.wav"
    },
    { // 13
        "wavs/barber_starv1.wav", "wavs/barber_nojob1.wav", "wavs/barber_needjob1.wav", "wavs/barber_nofun1.wav",
        "wavs/barber_relig1.wav", "wavs/barber_great1.wav", "wavs/barber_great2.wav", "wavs/barber_exact1.wav",
        "wavs/barber_exact2.wav", "wavs/barber_exact3.wav", "wavs/barber_exact4.wav", "wavs/barber_exact5.wav",
        "wavs/barber_exact6.wav", "wavs/barber_exact7.wav", "wavs/barber_exact8.wav", "wavs/barber_exact9.wav",
        "wavs/barber_exact0.wav", "wavs/barber_free1.wav", "wavs/barber_free2.wav", "wavs/barber_free3.wav"
    },
    { // 14
        "wavs/actors_starv1.wav", "wavs/actors_nojob1.wav", "wavs/actors_needjob1.wav", "wavs/actors_nofun1.wav",
        "wavs/actors_relig1.wav", "wavs/actors_great1.wav", "wavs/actors_great2.wav", "wavs/actors_exact1.wav",
        "wavs/actors_exact2.wav", "wavs/actors_exact3.wav", "wavs/actors_exact4.wav", "wavs/actors_exact5.wav",
        "wavs/actors_exact6.wav", "wavs/actors_exact7.wav", "wavs/actors_exact8.wav", "wavs/actors_exact9.wav",
        "wavs/actors_exact0.wav", "wavs/actors_free1.wav", "wavs/actors_free2.wav", "wavs/actors_free3.wav"
    },
    { // 15
        "wavs/gladtr_starv1.wav", "wavs/gladtr_nojob1.wav", "wavs/gladtr_needjob1.wav", "wavs/gladtr_nofun1.wav",
        "wavs/gladtr_relig1.wav", "wavs/gladtr_great1.wav", "wavs/gladtr_great2.wav", "wavs/gladtr_exact1.wav",
        "wavs/gladtr_exact2.wav", "wavs/gladtr_exact3.wav", "wavs/gladtr_exact4.wav", "wavs/gladtr_exact5.wav",
        "wavs/gladtr_exact6.wav", "wavs/gladtr_exact7.wav", "wavs/gladtr_exact8.wav", "wavs/gladtr_exact9.wav",
        "wavs/gladtr_exact0.wav", "wavs/gladtr_free1.wav", "wavs/gladtr_free2.wav", "wavs/gladtr_free3.wav"
    },
    { // 16
        "wavs/liontr_starv1.wav", "wavs/liontr_nojob1.wav", "wavs/liontr_needjob1.wav", "wavs/liontr_nofun1.wav",
        "wavs/liontr_relig1.wav", "wavs/liontr_great1.wav", "wavs/liontr_great2.wav", "wavs/liontr_exact1.wav",
        "wavs/liontr_exact2.wav", "wavs/liontr_exact3.wav", "wavs/liontr_exact4.wav", "wavs/liontr_exact5.wav",
        "wavs/liontr_exact6.wav", "wavs/liontr_exact7.wav", "wavs/liontr_exact8.wav", "wavs/liontr_exact9.wav",
        "wavs/liontr_exact0.wav", "wavs/liontr_free1.wav", "wavs/liontr_free2.wav", "wavs/liontr_free3.wav"
    },
    { // 17
        "wavs/charot_starv1.wav", "wavs/charot_nojob1.wav", "wavs/charot_needjob1.wav", "wavs/charot_nofun1.wav",
        "wavs/charot_relig1.wav", "wavs/charot_great1.wav", "wavs/charot_great2.wav", "wavs/charot_exact1.wav",
        "wavs/charot_exact2.wav", "wavs/charot_exact3.wav", "wavs/charot_exact4.wav", "wavs/charot_exact5.wav",
        "wavs/charot_exact6.wav", "wavs/charot_exact7.wav", "wavs/charot_exact8.wav", "wavs/charot_exact9.wav",
        "wavs/charot_exact0.wav", "wavs/charot_free1.wav", "wavs/charot_free2.wav", "wavs/charot_free3.wav"
    },
    { // 18
        "wavs/patric_starv1.wav", "wavs/patric_nojob1.wav", "wavs/patric_needjob1.wav", "wavs/patric_nofun1.wav",
        "wavs/patric_relig1.wav", "wavs/patric_great1.wav", "wavs/patric_great2.wav", "wavs/patric_exact1.wav",
        "wavs/patric_exact2.wav", "wavs/patric_exact3.wav", "wavs/patric_exact4.wav", "wavs/patric_exact5.wav",
        "wavs/patric_exact6.wav", "wavs/patric_exact7.wav", "wavs/patric_exact8.wav", "wavs/patric_exact9.wav",
        "wavs/patric_exact0.wav", "wavs/patric_free1.wav", "wavs/patric_free2.wav", "wavs/patric_free3.wav"
    },
    { // 19
        "wavs/pleb_starv1.wav", "wavs/pleb_nojob1.wav", "wavs/pleb_needjob1.wav", "wavs/pleb_nofun1.wav",
        "wavs/pleb_relig1.wav", "wavs/pleb_great1.wav", "wavs/pleb_great2.wav", "wavs/pleb_exact1.wav",
        "wavs/pleb_exact2.wav", "wavs/pleb_exact3.wav", "wavs/pleb_exact4.wav", "wavs/pleb_exact5.wav",
        "wavs/pleb_exact6.wav", "wavs/pleb_exact7.wav", "wavs/pleb_exact8.wav", "wavs/pleb_exact9.wav",
        "wavs/pleb_exact0.wav", "wavs/pleb_free1.wav", "wavs/pleb_free2.wav", "wavs/pleb_free3.wav"
    },
    { // 20
        "wavs/rioter_starv1.wav", "wavs/rioter_nojob1.wav", "wavs/rioter_needjob1.wav", "wavs/rioter_nofun1.wav",
        "wavs/rioter_relig1.wav", "wavs/rioter_great1.wav", "wavs/rioter_great2.wav", "wavs/rioter_exact1.wav",
        "wavs/rioter_exact2.wav", "wavs/rioter_exact3.wav", "wavs/rioter_exact4.wav", "wavs/rioter_exact5.wav",
        "wavs/rioter_exact6.wav", "wavs/rioter_exact7.wav", "wavs/rioter_exact8.wav", "wavs/rioter_exact9.wav",
        "wavs/rioter_exact0.wav", "wavs/rioter_free1.wav", "wavs/rioter_free2.wav", "wavs/rioter_free3.wav"
    },
    { // 21
        "wavs/homeless_starv1.wav", "wavs/homeless_nojob1.wav", "wavs/homeless_needjob1.wav", "wavs/homeless_nofun1.wav",
        "wavs/homeless_relig1.wav", "wavs/homeless_great1.wav", "wavs/homeless_great2.wav", "wavs/homeless_exact1.wav",
        "wavs/homeless_exact2.wav", "wavs/homeless_exact3.wav", "wavs/homeless_exact4.wav", "wavs/homeless_exact5.wav",
        "wavs/homeless_exact6.wav", "wavs/homeless_exact7.wav", "wavs/homeless_exact8.wav", "wavs/homeless_exact9.wav",
        "wavs/homeless_exact0.wav", "wavs/homeless_free1.wav", "wavs/homeless_free2.wav", "wavs/homeless_free3.wav"
    },
    { // 22
        "wavs/unemploy_starv1.wav", "wavs/unemploy_nojob1.wav", "wavs/unemploy_needjob1.wav", "wavs/unemploy_nofun1.wav",
        "wavs/unemploy_relig1.wav", "wavs/unemploy_great1.wav", "wavs/unemploy_great2.wav", "wavs/unemploy_exact1.wav",
        "wavs/unemploy_exact2.wav", "wavs/unemploy_exact3.wav", "wavs/unemploy_exact4.wav", "wavs/unemploy_exact5.wav",
        "wavs/unemploy_exact6.wav", "wavs/unemploy_exact7.wav", "wavs/unemploy_exact8.wav", "wavs/unemploy_exact9.wav",
        "wavs/unemploy_exact0.wav", "wavs/unemploy_free1.wav", "wavs/unemploy_free2.wav", "wavs/unemploy_free3.wav"
    },
    { // 23
        "wavs/emigrate_starv1.wav", "wavs/emigrate_nojob1.wav", "wavs/emigrate_needjob1.wav", "wavs/emigrate_nofun1.wav",
        "wavs/emigrate_relig1.wav", "wavs/emigrate_great1.wav", "wavs/emigrate_great2.wav", "wavs/emigrate_exact1.wav",
        "wavs/emigrate_exact2.wav", "wavs/emigrate_exact3.wav", "wavs/emigrate_exact4.wav", "wavs/emigrate_exact5.wav",
        "wavs/emigrate_exact6.wav", "wavs/emigrate_exact7.wav", "wavs/emigrate_exact8.wav", "wavs/emigrate_exact9.wav",
        "wavs/emigrate_exact0.wav", "wavs/emigrate_free1.wav", "wavs/emigrate_free2.wav", "wavs/emigrate_free3.wav"
    },
    { // 24
        "wavs/immigrant_starv1.wav", "wavs/immigrant_nojob1.wav", "wavs/immigrant_needjob1.wav", "wavs/immigrant_nofun1.wav",
        "wavs/immigrant_relig1.wav", "wavs/immigrant_great1.wav", "wavs/immigrant_great2.wav", "wavs/immigrant_exact1.wav",
        "wavs/immigrant_exact2.wav", "wavs/immigrant_exact3.wav", "wavs/immigrant_exact4.wav", "wavs/immigrant_exact5.wav",
        "wavs/immigrant_exact6.wav", "wavs/immigrant_exact7.wav", "wavs/immigrant_exact8.wav", "wavs/immigrant_exact9.wav",
        "wavs/immigrant_exact0.wav", "wavs/immigrant_free1.wav", "wavs/immigrant_free2.wav", "wavs/immigrant_free3.wav"
    },
    { // 25
        "wavs/enemy_starv1.wav", "wavs/enemy_nojob1.wav", "wavs/enemy_needjob1.wav", "wavs/enemy_nofun1.wav",
        "wavs/enemy_relig1.wav", "wavs/enemy_great1.wav", "wavs/enemy_great2.wav", "wavs/enemy_exact1.wav",
        "wavs/enemy_exact2.wav", "wavs/enemy_exact3.wav", "wavs/enemy_exact4.wav", "wavs/enemy_exact5.wav",
        "wavs/enemy_exact6.wav", "wavs/enemy_exact7.wav", "wavs/enemy_exact8.wav", "wavs/enemy_exact9.wav",
        "wavs/enemy_exact0.wav", "wavs/enemy_free1.wav", "wavs/enemy_free2.wav", "wavs/enemy_free3.wav"
    },
    { // 26
        "wavs/local_starv1.wav", "wavs/local_nojob1.wav", "wavs/local_needjob1.wav", "wavs/local_nofun1.wav",
        "wavs/local_relig1.wav", "wavs/local_great1.wav", "wavs/local_great2.wav", "wavs/local_exact1.wav",
        "wavs/local_exact2.wav", "wavs/local_exact3.wav", "wavs/local_exact4.wav", "wavs/local_exact5.wav",
        "wavs/local_exact6.wav", "wavs/local_exact7.wav", "wavs/local_exact8.wav", "wavs/local_exact9.wav",
        "wavs/local_exact0.wav", "wavs/local_free1.wav", "wavs/local_free2.wav", "wavs/local_free3.wav"
    },
    { // 27
        "wavs/libary_starv1.wav", "wavs/libary_nojob1.wav", "wavs/libary_needjob1.wav", "wavs/libary_nofun1.wav",
        "wavs/libary_relig1.wav", "wavs/libary_great1.wav", "wavs/libary_great2.wav", "wavs/libary_exact1.wav",
        "wavs/libary_exact2.wav", "wavs/libary_exact3.wav", "wavs/libary_exact4.wav", "wavs/libary_exact5.wav",
        "wavs/libary_exact6.wav", "wavs/libary_exact7.wav", "wavs/libary_exact8.wav", "wavs/libary_exact9.wav",
        "wavs/libary_exact0.wav", "wavs/libary_free1.wav", "wavs/libary_free2.wav", "wavs/libary_free3.wav"
    },
    { // 28
        "wavs/srgeon_starv1.wav", "wavs/srgeon_nojob1.wav", "wavs/srgeon_needjob1.wav", "wavs/srgeon_nofun1.wav",
        "wavs/srgeon_relig1.wav", "wavs/srgeon_great1.wav", "wavs/srgeon_great2.wav", "wavs/srgeon_exact1.wav",
        "wavs/srgeon_exact2.wav", "wavs/srgeon_exact3.wav", "wavs/srgeon_exact4.wav", "wavs/srgeon_exact5.wav",
        "wavs/srgeon_exact6.wav", "wavs/srgeon_exact7.wav", "wavs/srgeon_exact8.wav", "wavs/srgeon_exact9.wav",
        "wavs/srgeon_exact0.wav", "wavs/srgeon_free1.wav", "wavs/srgeon_free2.wav", "wavs/srgeon_free3.wav"
    },
    { // 29
        "wavs/docker_starv1.wav", "wavs/docker_nojob1.wav", "wavs/docker_needjob1.wav", "wavs/docker_nofun1.wav",
        "wavs/docker_relig1.wav", "wavs/docker_great1.wav", "wavs/docker_great2.wav", "wavs/docker_exact1.wav",
        "wavs/docker_exact2.wav", "wavs/docker_exact3.wav", "wavs/docker_exact4.wav", "wavs/docker_exact5.wav",
        "wavs/docker_exact6.wav", "wavs/docker_exact7.wav", "wavs/docker_exact8.wav", "wavs/docker_exact9.wav",
        "wavs/docker_exact0.wav", "wavs/docker_free1.wav", "wavs/docker_free2.wav", "wavs/docker_free3.wav"
    },
    { // 30
        "wavs/missionary_starv1.wav", "wavs/missionary_nojob1.wav", "wavs/missionary_needjob1.wav", "wavs/missionary_nofun1.wav",
        "wavs/missionary_relig1.wav", "wavs/missionary_great1.wav", "wavs/missionary_great2.wav", "wavs/missionary_exact1.wav",
        "wavs/missionary_exact2.wav", "wavs/missionary_exact3.wav", "wavs/mission_exact4.wav", "wavs/missionary_exact5.wav",
        "wavs/missionary_exact6.wav", "wavs/missionary_exact7.wav", "wavs/missionary_exact8.wav", "wavs/missionary_exact9.wav",
        "wavs/missionary_exact0.wav", "wavs/missionary_free1.wav", "wavs/missionary_free2.wav", "wavs/missionary_free3.wav"
    },
    { // 31
        "wavs/granboy_starv1.wav", "wavs/granboy_nojob1.wav", "wavs/granboy_needjob1.wav", "wavs/granboy_nofun1.wav",
        "wavs/granboy_relig1.wav", "wavs/granboy_great1.wav", "wavs/granboy_great2.wav", "wavs/granboy_exact1.wav",
        "wavs/granboy_exact2.wav", "wavs/granboy_exact3.wav", "wavs/granboy_exact4.wav", "wavs/granboy_exact5.wav",
        "wavs/granboy_exact6.wav", "wavs/granboy_exact7.wav", "wavs/granboy_exact8.wav", "wavs/granboy_exact9.wav",
        "wavs/granboy_exact0.wav", "wavs/granboy_free1.wav", "wavs/granboy_free2.wav", "wavs/granboy_free3.wav"
    },
    { // 32 FIGURE_DEPOT_CART_PUSHER = 91
        "wavs/ox_starv1.wav", "wavs/ox_nojob1.wav", "wavs/ox_needjob1.wav", "wavs/ox_nofun1.wav",
        "wavs/ox_relig1.wav", "wavs/ox_great1.wav", "wavs/ox_great2.wav", ASSETS_DIRECTORY "/Sounds/Ox.ogg",
        "wavs/ox_exact2.wav", "wavs/ox_exact3.wav", "wavs/ox_exact4.wav", "wavs/ox_exact5.wav",
        "wavs/ox_exact6.wav", "wavs/ox_exact7.wav", "wavs/ox_exact8.wav", "wavs/ox_exact9.wav",
        "wavs/ox_exact0.wav", "wavs/ox_free1.wav", "wavs/ox_free2.wav", "wavs/ox_free3.wav"
    },
};

static const int FIGURE_TYPE_TO_SOUND_TYPE[] = {
    -1, 24, 23, 21, 5, 19, -1, 3, 2, 5, // 0-9
    0, 1, 1, 1, -1, 14, 15, 16, 17, 6, // 10-19
    7, 6, 20, 20, 20, -1, 4, 8, 10, 9, // 20-29
    9, 13, 11, 12, 12, 19, -1, -1, 5, 4, // 30-39
    18, -1, 1, 25, 25, 25, 25, 25, 25, 25, // 40-49
    25, 25, 25, 25, 25, 25, 25, 25, -1, -1, // 50-59
    -1, -1, -1, -1, 30, -1, 31, -1, -1, -1, // 60-69
    -1, -1, -1, 19, 19, 2, 1, 19, 8, 11,  // 70-79
    11, -1, 1, -1, -1, 19, 20, 20, 19, 19,  // 80-89
    19, 32, -1, 22, 25, -1, -1, -1, -1, -1, // 90-99
};

enum {
    GOD_STATE_NONE = 0,
    GOD_STATE_VERY_ANGRY = 1,
    GOD_STATE_ANGRY = 2
};

static void play_sound_file(int sound_id, int phrase_id)
{
    if (sound_id >= 0 && phrase_id >= 0) {
        sound_speech_play_file(FIGURE_SOUNDS[sound_id][phrase_id]);
    }
}

int figure_phrase_play(figure *f)
{
    if (f->id <= 0) {
        return 0;
    }
    int sound_id = FIGURE_TYPE_TO_SOUND_TYPE[f->type];
    play_sound_file(sound_id, f->phrase_id);
    return sound_id;
}

static int lion_tamer_phrase(figure *f)
{
    if (f->action_state == FIGURE_ACTION_150_ATTACK) {
        if (++f->phrase_sequence_exact >= 3) {
            f->phrase_sequence_exact = 0;
        }
        return 7 + f->phrase_sequence_exact;
    }
    return -1;
}

static int gladiator_phrase(figure *f)
{
    return f->action_state == FIGURE_ACTION_150_ATTACK ? 7 : -1;
}

static int tax_collector_phrase(figure *f)
{
    if (f->min_max_seen >= HOUSE_LARGE_CASA) {
        return 7;
    } else if (f->min_max_seen >= HOUSE_SMALL_HOVEL) {
        return 8;
    } else if (f->min_max_seen >= HOUSE_LARGE_TENT) {
        return 9;
    } else {
        return -1;
    }
}

static int market_trader_phrase(figure *f)
{
    if (f->action_state == FIGURE_ACTION_126_ROAMER_RETURNING) {
        if (building_market_get_max_food_stock(building_get(f->building_id)) <= 0) {
            return 9; // run out of goods
        }
    }
    return -1;
}

static int market_supplier_phrase(figure *f)
{
    if (f->action_state == FIGURE_ACTION_145_SUPPLIER_GOING_TO_STORAGE) {
        return 7;
    } else if (f->action_state == FIGURE_ACTION_146_SUPPLIER_RETURNING) {
        resource_type resource = f->collecting_item_id;
        if (resource != RESOURCE_NONE) {
            return 8;
        } else {
            return -1;
        }
    } else {
        return -1;
    }
}

static int cart_pusher_phrase(figure *f)
{
    if (f->action_state == FIGURE_ACTION_20_CARTPUSHER_INITIAL) {
        if (f->min_max_seen == 2) {
            return 7;
        } else if (f->min_max_seen == 1) {
            return 8;
        }
    } else if (f->action_state == FIGURE_ACTION_21_CARTPUSHER_DELIVERING_TO_WAREHOUSE ||
            f->action_state == FIGURE_ACTION_22_CARTPUSHER_DELIVERING_TO_GRANARY ||
            f->action_state == FIGURE_ACTION_23_CARTPUSHER_DELIVERING_TO_WORKSHOP) {
        if (calc_maximum_distance(
            f->destination_x, f->destination_y, f->source_x, f->source_y) >= 25) {
            return 9; // too far
        }
    }
    return -1;
}

static int mess_hall_supplier_phrase(figure *f)
{
    return 0;
}

static int warehouseman_phrase(figure *f)
{
    if (f->action_state == FIGURE_ACTION_51_WAREHOUSEMAN_DELIVERING_RESOURCE) {
        if (calc_maximum_distance(
            f->destination_x, f->destination_y, f->source_x, f->source_y) >= 25) {
            return 9; // too far
        }
    }
    return -1;
}

static int prefect_phrase(figure *f)
{
    if (++f->phrase_sequence_exact >= 4) {
        f->phrase_sequence_exact = 0;
    }
    if (f->action_state == FIGURE_ACTION_74_PREFECT_GOING_TO_FIRE) {
        return 10;
    } else if (f->action_state == FIGURE_ACTION_75_PREFECT_AT_FIRE) {
        return 11 + (f->phrase_sequence_exact % 2);
    } else if (f->action_state == FIGURE_ACTION_150_ATTACK) {
        return 13 + f->phrase_sequence_exact;
    } else if (f->min_max_seen >= 50) {
        // alternate between "no sign of crime around here" and the regular city phrases
        if (f->phrase_sequence_exact % 2) {
            return 7;
        } else {
            return -1;
        }
    } else if (f->min_max_seen >= 10) {
        return 8;
    } else {
        return 9;
    }
}

static int engineer_phrase(figure *f)
{
    if (f->min_max_seen >= 60) {
        return 7;
    } else if (f->min_max_seen >= 10) {
        return 8;
    } else {
        return -1;
    }
}

static int citizen_phrase(figure *f)
{
    if (++f->phrase_sequence_exact >= 3) {
        f->phrase_sequence_exact = 0;
    }
    return 7 + f->phrase_sequence_exact;
}

static int missionary_phrase(figure *f)
{
    if (++f->phrase_sequence_exact >= 4) {
        f->phrase_sequence_exact = 0;
    }
    return 7 + f->phrase_sequence_exact;
}

static int ox_phrase(figure *f)
{
    if (++f->phrase_sequence_exact >= 1) {
        f->phrase_sequence_exact = 0;
    }
    return 7 + f->phrase_sequence_exact;
}

static int homeless_phrase(figure *f)
{
    if (++f->phrase_sequence_exact >= 2) {
        f->phrase_sequence_exact = 0;
    }
    return 7 + f->phrase_sequence_exact;
}

static int house_seeker_phrase(figure *f)
{
    if (++f->phrase_sequence_exact >= 3) {
        f->phrase_sequence_exact = 0;
    }
    return 7 + f->phrase_sequence_exact;
}

static int emigrant_phrase(void)
{
    switch (city_sentiment_low_mood_cause()) {
        case LOW_MOOD_CAUSE_NO_JOBS:
            return 7;
        case LOW_MOOD_CAUSE_NO_FOOD:
            return 8;
        case LOW_MOOD_CAUSE_HIGH_TAXES:
            return 9;
        case LOW_MOOD_CAUSE_LOW_WAGES:
            return 10;
        default:
            return 11;
    }
}

static int tower_sentry_phrase(figure *f)
{
    if (++f->phrase_sequence_exact >= 2) {
        f->phrase_sequence_exact = 0;
    }
    int enemies = city_figures_enemies();
    if (!enemies) {
        return 7 + f->phrase_sequence_exact;
    } else if (enemies <= 10) {
        return 9;
    } else if (enemies <= 30) {
        return 10;
    } else {
        return 11;
    }
}

static int soldier_phrase(void)
{
    int enemies = city_figures_enemies();
    if (enemies >= 40) {
        return 11;
    } else if (enemies > 20) {
        return 10;
    } else if (enemies) {
        return 9;
    }
    return -1;
}

static int docker_phrase(figure *f)
{
    if (f->action_state == FIGURE_ACTION_135_DOCKER_IMPORT_GOING_TO_STORAGE ||
        f->action_state == FIGURE_ACTION_136_DOCKER_EXPORT_GOING_TO_STORAGE) {
        if (calc_maximum_distance(
            f->destination_x, f->destination_y, f->source_x, f->source_y) >= 25) {
            return 9; // too far
        }
    }
    return -1;
}

static int trade_caravan_phrase(figure *f)
{
    if (++f->phrase_sequence_exact >= 2) {
        f->phrase_sequence_exact = 0;
    }
    if (f->action_state == FIGURE_ACTION_103_TRADE_CARAVAN_LEAVING) {
        if (!trader_has_traded(f->trader_id)) {
            return 7; // no trade
        }
    } else if (f->action_state == FIGURE_ACTION_102_TRADE_CARAVAN_TRADING) {
        if (figure_trade_caravan_can_buy(f, f->destination_building_id, f->empire_city_id)) {
            return 11; // buying goods
        } else if (figure_trade_caravan_can_sell(f, f->destination_building_id, f->empire_city_id)) {
            return 10; // selling goods
        }
    }
    return 8 + f->phrase_sequence_exact;
}

static int trade_ship_phrase(figure *f)
{
    if (f->action_state == FIGURE_ACTION_115_TRADE_SHIP_LEAVING) {
        if (!trader_has_traded(f->trader_id)) {
            return 9; // no trade
        } else {
            return 11; // good trade
        }
    } else if (f->action_state == FIGURE_ACTION_112_TRADE_SHIP_MOORED) {
        int state = figure_trade_ship_is_trading(f);
        if (state == TRADE_SHIP_BUYING) {
            return 8; // buying goods
        } else if (state == TRADE_SHIP_SELLING) {
            return 7; // selling goods
        } else {
            if (!trader_has_traded(f->trader_id)) {
                return 9; // no trade
            } else {
                return 11; // good trade
            }
        }
    } else {
        if (!trader_has_traded(f->trader_id)) {
            return 10; // can't wait to trade
        } else {
            return 11; // good trade
        }
    }
}

static int city_god_state(void)
{
    int least_god_happiness = 100;
    for (int i = 0; i < MAX_GODS; i++) {
        int happiness = city_god_happiness(i);
        if (happiness < least_god_happiness) {
            least_god_happiness = happiness;
        }
    }
    if (least_god_happiness < 20) {
        return GOD_STATE_VERY_ANGRY;
    } else if (least_god_happiness < 40) {
        return GOD_STATE_ANGRY;
    } else {
        return GOD_STATE_NONE;
    }
}

static int barkeep_phrase(figure *f)
{
    f->phrase_sequence_city = 0;
    int god_state = city_god_state();
    int unemployment_pct = city_labor_unemployment_percentage();

    if (unemployment_pct >= 17) {
        return 1;
    } else if (city_labor_workers_needed() >= 10) {
        return 2;
    } else if (city_culture_average_entertainment() == 0) {
        return 3;
    } else if (god_state == GOD_STATE_VERY_ANGRY) {
        return 4;
    } else if (city_culture_average_entertainment() <= 10) {
        return 3;
    } else if (god_state == GOD_STATE_ANGRY) {
        return 4;
    } else if (city_culture_average_entertainment() <= 20) {
        return 3;
    } else if (city_resource_food_supply_months() >= 4 &&
        unemployment_pct <= 5 &&
        city_culture_average_health() > 0 &&
        city_culture_average_education() > 0) {
        if (city_population() < 500) {
            return 5;
        } else {
            return 6;
        }
    } else if (unemployment_pct >= 10) {
        return 1;
    } else {
        return 5;
    }
}

static int beggar_phrase(figure *f)
{
    if (++f->phrase_sequence_exact >= 2) {
        f->phrase_sequence_exact = 0;
    }
    return 7 + f->phrase_sequence_exact;
}

static int phrase_based_on_figure_state(figure *f)
{
    switch (f->type) {
        case FIGURE_LION_TAMER:
            return lion_tamer_phrase(f);
        case FIGURE_GLADIATOR:
            return gladiator_phrase(f);
        case FIGURE_TAX_COLLECTOR:
            return tax_collector_phrase(f);
        case FIGURE_MARKET_TRADER:
            return market_trader_phrase(f);
        case FIGURE_MARKET_SUPPLIER:
            return market_supplier_phrase(f);
        case FIGURE_CART_PUSHER:
            return cart_pusher_phrase(f);
        case FIGURE_WAREHOUSEMAN:
            return warehouseman_phrase(f);
        case FIGURE_PREFECT:
            return prefect_phrase(f);
        case FIGURE_ENGINEER:
        case FIGURE_WORK_CAMP_ARCHITECT:
            return engineer_phrase(f);
        case FIGURE_PROTESTER:
        case FIGURE_CRIMINAL:
        case FIGURE_RIOTER:
        case FIGURE_CRIMINAL_ROBBER:
        case FIGURE_CRIMINAL_LOOTER:
        case FIGURE_DELIVERY_BOY:
            return citizen_phrase(f);
        case FIGURE_MISSIONARY:
            return missionary_phrase(f);
        case FIGURE_DEPOT_CART_PUSHER:
            return ox_phrase(f);
        case FIGURE_HOMELESS:
            return homeless_phrase(f);
        case FIGURE_IMMIGRANT:
            return house_seeker_phrase(f);
        case FIGURE_EMIGRANT:
            return emigrant_phrase();
        case FIGURE_TOWER_SENTRY:
        case FIGURE_WATCHMAN:
            return tower_sentry_phrase(f);
        case FIGURE_MESS_HALL_SUPPLIER:
            return mess_hall_supplier_phrase(f);
        case FIGURE_FORT_JAVELIN:
        case FIGURE_FORT_MOUNTED:
        case FIGURE_FORT_LEGIONARY:
        case FIGURE_FORT_INFANTRY:
        case FIGURE_FORT_ARCHER:
            return soldier_phrase();
        case FIGURE_DOCKER:
            return docker_phrase(f);
        case FIGURE_TRADE_CARAVAN:
            return trade_caravan_phrase(f);
        case FIGURE_BARKEEP:
        case FIGURE_BARKEEP_SUPPLIER:
            return barkeep_phrase(f);
        case FIGURE_TRADE_CARAVAN_DONKEY:
            while (f->type == FIGURE_TRADE_CARAVAN_DONKEY && f->leading_figure_id) {
                f = figure_get(f->leading_figure_id);
            }
            return f->type == FIGURE_TRADE_CARAVAN ? trade_caravan_phrase(f) : -1;
        case FIGURE_TRADE_SHIP:
            return trade_ship_phrase(f);
        case FIGURE_BEGGAR:
            return beggar_phrase(f);
    }
    return -1;
}

static int phrase_based_on_city_state(figure *f)
{
    f->phrase_sequence_city = 0;
    int god_state = city_god_state();
    int unemployment_pct = city_labor_unemployment_percentage();

    if (city_resource_food_supply_months() <= 0) {
        return 0;
    } else if (unemployment_pct >= 17) {
        return 1;
    } else if (city_labor_workers_needed() >= 10) {
        return 2;
    } else if (city_culture_average_entertainment() == 0) {
        return 3;
    } else if (god_state == GOD_STATE_VERY_ANGRY) {
        return 4;
    } else if (city_culture_average_entertainment() <= 10) {
        return 3;
    } else if (god_state == GOD_STATE_ANGRY) {
        return 4;
    } else if (city_culture_average_entertainment() <= 20) {
        return 3;
    } else if (city_resource_food_supply_months() >= 4 &&
            unemployment_pct <= 5 &&
            city_culture_average_health() > 0 &&
            city_culture_average_education() > 0) {
        if (city_population() < 500) {
            return 5;
        } else {
            return 6;
        }
    } else if (unemployment_pct >= 10) {
        return 1;
    } else {
        return 5;
    }
}

void figure_phrase_determine(figure *f)
{
    if (f->id <= 0) {
        return;
    }
    f->phrase_id = 0;

    if (figure_is_enemy(f) || f->type == FIGURE_INDIGENOUS_NATIVE || f->type == FIGURE_NATIVE_TRADER) {
        f->phrase_id = -1;
        return;
    }

    int phrase_id = phrase_based_on_figure_state(f);
    if (phrase_id != -1) {
        f->phrase_id = phrase_id;
    } else {
        f->phrase_id = phrase_based_on_city_state(f);
    }
}
