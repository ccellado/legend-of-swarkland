#ifndef SWARKLAND_HPP
#define SWARKLAND_HPP

#include "hashtable.hpp"
#include "geometry.hpp"
#include "list.hpp"
#include "individual.hpp"

struct Action {
    enum Type {
        MOVE,
        WAIT,
        ATTACK,

        CHEATCODE_HEALTH_BOOST,
        CHEATCODE_KILL_EVERYBODY_IN_THE_WORLD,
        CHEATCODE_POLYMORPH,
        CHEATCODE_INVISIBILITY,
        CHEATCODE_GENERATE_MONSTER,

        // only a player can be undecided
        UNDECIDED,
    };
    Type type;
    Coord coord;

    // canonical singletons, appropriate for == comparison
    static inline Action wait() {
        return {WAIT, {0, 0}};
    }
    static inline Action undecided() {
        return {UNDECIDED, {0, 0}};
    }

    static inline Action cheatcode_health_boost() {
        return {CHEATCODE_HEALTH_BOOST, {0, 0}};
    }
    static inline Action cheatcode_kill_everybody_in_the_world() {
        return {CHEATCODE_KILL_EVERYBODY_IN_THE_WORLD, {0, 0}};
    }
    static inline Action cheatcode_polymorph() {
        return {CHEATCODE_POLYMORPH, {0, 0}};
    }
    static inline Action cheatcode_invisibility() {
        return {CHEATCODE_INVISIBILITY, {0, 0}};
    }
    static inline Action cheatcode_generate_monster() {
        return {CHEATCODE_GENERATE_MONSTER, {0, 0}};
    }
};
static inline bool operator==(Action a, Action b) {
    return a.type == b.type && a.coord == b.coord;
}
static inline bool operator!=(Action a, Action b) {
    return !(a == b);
}

struct Event {
    enum Type {
        MOVE,
        ATTACK,
        DIE,
        // spawn or become visible
        APPEAR,
        // these are possible with cheatcodes
        DISAPPEAR,
        POLYMORPH,
    };
    Type type;
    Individual individual1;
    Individual individual2;
    Coord coord1;
    Coord coord2;
    static inline Event move(Individual mover, Coord from, Coord to) {
        return {
            MOVE,
            mover,
            NULL,
            from,
            to,
        };
    }
    static inline Event attack(Individual attacker, Individual target) {
        return {
            ATTACK,
            attacker,
            target,
            attacker->location,
            target->location,
        };
    }
    static inline Event die(Individual deceased) {
        return single_individual_event(DIE, deceased);
    }
    static inline Event appear(Individual new_guy) {
        return single_individual_event(APPEAR, new_guy);
    }
    static inline Event disappear(Individual cant_see_me) {
        return single_individual_event(DISAPPEAR, cant_see_me);
    }
    static inline Event polymorph(Individual shapeshifter) {
        return single_individual_event(POLYMORPH, shapeshifter);
    }
private:
    static inline Event single_individual_event(Type type, Individual individual) {
        return {
            type,
            individual,
            NULL,
            individual->location,
            Coord::nowhere(),
        };
    }
};

extern Species specieses[SpeciesId_COUNT];

extern IdMap<Individual> individuals;
extern Individual you;
extern bool youre_still_alive;
extern long long time_counter;

extern bool cheatcode_full_visibility;
extern Individual cheatcode_spectator;
void cheatcode_spectate(Coord individual_at);

void swarkland_init();

void get_available_actions(Individual individual, List<Action> & output_actions);

Individual spawn_a_monster(SpeciesId species_id, Team team, DecisionMakerType decision_maker);
void run_the_game();
PerceivedIndividual find_perceived_individual_at(Individual observer, Coord location);
Individual find_individual_at(Coord location);

#endif
