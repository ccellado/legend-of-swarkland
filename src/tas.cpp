#include "tas.hpp"

#include "random.hpp"

#include <stdio.h>
#include <errno.h>

int tas_delay;

static const char * script_path;
static FILE * script_file;
static uint32_t tas_seed;
static int frame_counter;
static TasScriptMode current_mode;

void set_tas_delay(int n) {
    tas_delay = n;
}
uint32_t tas_get_seed() {
    return tas_seed;
}

class Token {
public:
    String string = nullptr;
    // col starts at 1
    int col = 0;
};

static void tokenize_line(const String & line, List<Token> * tokens) {
    int index = 0;
    Token current_token;
    for (; index < line->length(); index++) {
        uint32_t c = (*line)[index];
        if (c == '#')
            break;
        if (current_token.col == 0) {
            if (c == ' ')
                continue;
            // start token
            current_token.col = index + 1;
        } else {
            if (c != ' ')
                continue;
            // end token
            current_token.string = line->substring(current_token.col - 1, index);
            tokens->append(current_token);
            current_token = Token();
        }
    }
    if (current_token.col != 0) {
        // end token
        current_token.string = line->substring(current_token.col - 1, index);
        tokens->append(current_token);
    }
}

static void write_line(const String & line) {
    ByteBuffer buffer;
    line->encode(&buffer);
    if (fwrite(buffer.raw(), buffer.length(), 1, script_file) < 1) {
        fprintf(stderr, "ERROR: IO error when writing to file: %s\n", script_path);
        exit(1);
    }
    fflush(script_file);
}
// line_number starts at 1
int line_number = 0;
ByteBuffer read_buffer;
static String read_line() {
    int index = 0;
    while (true) {
        // look for an EOL
        for (; index < read_buffer.length(); index++) {
            if (index >= 256) {
                fprintf(stderr, "ERROR: line length too long: %s\n", script_path);
            }
            if (read_buffer[index] != '\n')
                continue;
            line_number++;
            String line = new_string();
            bool ok;
            line->decode(read_buffer, 0, index, &ok);
            if (!ok) {
                fprintf(stderr, "ERROR: unable to decode file as UTF-8: %s\n", script_path);
                exit(1);
            }
            read_buffer.remove_range(0, index + 1);
            return line;
        }

        // read more chars
        char blob[256];
        size_t read_count = fread(blob, 1, 256, script_file);
        if (read_count == 0) {
            if (read_buffer.length() > 0) {
                fprintf(stderr, "ERROR: expected newline at end of file: %s\n", script_path);
                exit(1);
            }
            return nullptr;
        }
        read_buffer.append(blob, read_count);
    }
}
__attribute__((noreturn))
static void report_error(const Token & token, int start, const char * msg) {
    int col = token.col + start;
    fprintf(stderr, "%s:%d:%d: error: %s\n", script_path, line_number, col, msg);
    exit(1);
}
__attribute__((noreturn))
static void report_error(const Token & token, int start, const char * msg1, const char * msg2) {
    int col = token.col + start;
    fprintf(stderr, "%s:%d:%d: error: %s%s\n", script_path, line_number, col, msg1, msg2);
    exit(1);
}

static String uint32_to_string(uint32_t n) {
    // python: hex(n)[2:].zfill(8)
    String string = new_string();
    for (int i = 0; i < 8; i++) {
        uint32_t nibble = (n >> (32 - (i + 1) * 4)) & 0xf;
        char c = nibble <= 9 ? nibble + '0' : (nibble - 0xa) + 'a';
        string->append(c);
    }
    return string;
}
template<int Size64>
static String uint_oversized_to_string(uint_oversized<Size64> n) {
    String string = new_string();
    for (int j = 0; j < Size64; j++) {
        for (int i = 0; i < 16; i++) {
            uint32_t nibble = (n.values[j] >> (64 - (i + 1) * 4)) & 0xf;
            char c = nibble <= 9 ? nibble + '0' : (nibble - 0xa) + 'a';
            string->append(c);
        }
    }
    return string;
}
static String uint256_to_string(uint256 n) {
    return uint_oversized_to_string(n);
}
static String int_to_string(int n) {
    String result = new_string();
    if (n == -2147483648) {
        // special case, because this value is unnegatable.
        result->append("-2147483648");
        return result;
    }
    if (n < 0) {
        result->append('-');
        n = -n;
    }
    bool print_now = false;
    for (int place_value = 1000000000; place_value > 0; place_value /= 10) {
        int digit = n / place_value;
        if (print_now || digit != 0) {
            result->append('0' + digit);
            n -= digit * place_value;
            print_now = true;
        }
    }
    if (!print_now) {
        result->append('0');
    }
    return result;
}

static uint32_t parse_nibble(const Token & token, int index) {
    uint32_t c = (*token.string)[index];
    if ('0' <= c && c <= '9') {
        return  c - '0';
    } else if ('a' <= c && c <= 'f') {
        return c - 'a' + 0xa;
    } else {
        report_error(token, index, "hex digit out of range [0-9a-f]");
    }
}
static int parse_int(const Token & token) {
    int index = 0;
    bool negative = false;
    if (token.string->length() >= 1 && (*token.string)[index] == '-') {
        negative = true;
        index++;
    }
    if (token.string->length() == index)
        report_error(token, index, "expected decimal digits");
    int x = 0;
    for (; index < token.string->length(); index++) {
        uint32_t c = (*token.string)[index];
        int digit = c - '0';
        if (digit > 9)
            report_error(token, index, "expected decimal digit");
        // x *= radix;
        if (__builtin_mul_overflow(x, 10, &x))
            report_error(token, index, "integer overflow");

        // x += digit
        if (__builtin_add_overflow(x, digit, &x))
            report_error(token, index, "integer overflow");
    }
    if (negative) {
        // x *= -1;
        if (__builtin_mul_overflow(x, -1, &x))
            report_error(token, 0, "integer overflow");
    }
    return x;
}
static uint32_t parse_uint32(const Token & token) {
    if (token.string->length() != 8)
        report_error(token, 0, "expected hex uint32");
    uint32_t n = 0;
    for (int i = 0; i < 8; i++) {
        uint32_t nibble = parse_nibble(token, i);
        n |= nibble << ((8 - i - 1) * 4);
    }
    return n;
}
template <int Size64>
static uint_oversized<Size64> parse_uint_oversized(const Token & token) {
    if (token.string->length() != 16 * Size64) {
        assert(Size64 == 4); // we'd need different error messages
        report_error(token, 0, "expected hex uint256");
    }
    uint_oversized<Size64> result;
    for (int j = 0; j < Size64; j++) {
        uint64_t n = 0;
        for (int i = 0; i < 16; i++) {
            uint64_t nibble = parse_nibble(token, j * 16 + i);
            n |= nibble << ((16 - i - 1) * 4);
        }
        result.values[j] = n;
    }
    return result;
}
static inline uint256 parse_uint256(const Token & token) {
    return parse_uint_oversized<4>(token);
}

static Coord parse_coord(const Token & token1, const Token & token2) {
    return Coord{parse_int(token1), parse_int(token2)};
}

static String action_names[Action::COUNT];
static String species_names[SpeciesId_COUNT];
static String decision_maker_names[DecisionMakerType_COUNT];
static String thing_type_names[ThingType_COUNT];
static String wand_id_names[WandId_COUNT];
static String potion_id_names[PotionId_COUNT];
static void check_no_nulls(String array[], int size) {
    for (int i = 0; i < size; i++)
        if (array[i] == nullptr)
            panic("missed a spot");
}
static void init_name_arrays() {
    action_names[Action::MOVE] = new_string("move");
    action_names[Action::WAIT] = new_string("wait");
    action_names[Action::ATTACK] = new_string("attack");
    action_names[Action::ZAP] = new_string("zap");
    action_names[Action::PICKUP] = new_string("pickup");
    action_names[Action::DROP] = new_string("drop");
    action_names[Action::QUAFF] = new_string("quaff");
    action_names[Action::THROW] = new_string("throw");
    action_names[Action::GO_DOWN] = new_string("down");
    action_names[Action::CHEATCODE_HEALTH_BOOST] = new_string("!health");
    action_names[Action::CHEATCODE_KILL_EVERYBODY_IN_THE_WORLD] = new_string("!kill");
    action_names[Action::CHEATCODE_POLYMORPH] = new_string("!polymorph");
    action_names[Action::CHEATCODE_GENERATE_MONSTER] = new_string("!monster");
    action_names[Action::CHEATCODE_WISH] = new_string("!wish");
    action_names[Action::CHEATCODE_IDENTIFY] = new_string("!identify");
    action_names[Action::CHEATCODE_GO_DOWN] = new_string("!down");
    action_names[Action::CHEATCODE_GAIN_LEVEL] = new_string("!levelup");
    check_no_nulls(action_names, Action::COUNT);

    species_names[SpeciesId_HUMAN] = new_string("human");
    species_names[SpeciesId_OGRE] = new_string("ogre");
    species_names[SpeciesId_LICH] = new_string("lich");
    species_names[SpeciesId_PINK_BLOB] = new_string("pink_blob");
    species_names[SpeciesId_AIR_ELEMENTAL] = new_string("air_elemenetal");
    species_names[SpeciesId_DOG] = new_string("dog");
    species_names[SpeciesId_ANT] = new_string("ant");
    species_names[SpeciesId_BEE] = new_string("bee");
    species_names[SpeciesId_BEETLE] = new_string("beetle");
    species_names[SpeciesId_SCORPION] = new_string("scorpion");
    species_names[SpeciesId_SNAKE] = new_string("snake");
    check_no_nulls(species_names, SpeciesId_COUNT);

    decision_maker_names[DecisionMakerType_PLAYER] = new_string("player");
    decision_maker_names[DecisionMakerType_AI] = new_string("ai");
    check_no_nulls(decision_maker_names, DecisionMakerType_COUNT);

    thing_type_names[ThingType_INDIVIDUAL] = new_string("individual");
    thing_type_names[ThingType_WAND] = new_string("wand");
    thing_type_names[ThingType_POTION] = new_string("potion");
    check_no_nulls(thing_type_names, ThingType_COUNT);

    wand_id_names[WandId_WAND_OF_CONFUSION] = new_string("confusion");
    wand_id_names[WandId_WAND_OF_DIGGING] = new_string("digging");
    wand_id_names[WandId_WAND_OF_STRIKING] = new_string("striking");
    wand_id_names[WandId_WAND_OF_SPEED] = new_string("speed");
    wand_id_names[WandId_WAND_OF_REMEDY] = new_string("remedy");
    check_no_nulls(wand_id_names, WandId_COUNT);

    potion_id_names[PotionId_POTION_OF_HEALING] = new_string("healing");
    potion_id_names[PotionId_POTION_OF_POISON] = new_string("poison");
    potion_id_names[PotionId_POTION_OF_ETHEREAL_VISION] = new_string("ethereal_vision");
    potion_id_names[PotionId_POTION_OF_COGNISCOPY] = new_string("cogniscopy");
    potion_id_names[PotionId_POTION_OF_BLINDNESS] = new_string("blindness");
    potion_id_names[PotionId_POTION_OF_INVISIBILITY] = new_string("invisibility");
    check_no_nulls(potion_id_names, PotionId_COUNT);
}

static Action::Id parse_action_type(const Token & token) {
    for (int i = 0; i < Action::COUNT; i++) {
        if (*action_names[i] == *token.string)
            return (Action::Id)i;
    }
    report_error(token, 0, "undefined action name");
}
static SpeciesId parse_species_id(const Token & token) {
    for (int i = 0; i < Action::COUNT; i++) {
        if (*species_names[i] == *token.string)
            return (SpeciesId)i;
    }
    report_error(token, 0, "undefined species id");
}
static DecisionMakerType parse_decision_maker(const Token & token) {
    for (int i = 0; i < DecisionMakerType_COUNT; i++) {
        if (*decision_maker_names[i] == *token.string)
            return (DecisionMakerType)i;
    }
    report_error(token, 0, "undefined decision maker");
}
static ThingType parse_thing_type(const Token & token) {
    for (int i = 0; i < ThingType_COUNT; i++) {
        if (*thing_type_names[i] == *token.string)
            return (ThingType)i;
    }
    report_error(token, 0, "undefined thing type");
}
static WandId parse_wand_id(const Token & token) {
    for (int i = 0; i < WandId_COUNT; i++) {
        if (*wand_id_names[i] == *token.string)
            return (WandId)i;
    }
    report_error(token, 0, "undefined wand id");
}
static PotionId parse_potion_id(const Token & token) {
    for (int i = 0; i < PotionId_COUNT; i++) {
        if (*potion_id_names[i] == *token.string)
            return (PotionId)i;
    }
    report_error(token, 0, "undefined potion id");
}

static const char * const RNG_DIRECTIVE = "@rng";
static String rng_input_to_string(const ByteBuffer & tag, int value) {
    String line = new_string();
    line->format("%s %d ", RNG_DIRECTIVE, value);
    line->decode(tag);
    line->append("\n");
    return line;
}
static int read_rng_input(const ByteBuffer & tag) {
    List<Token> tokens;
    while (tokens.length() == 0) {
        String line = read_line();
        if (line == nullptr)
            break; // and error
        tokenize_line(line, &tokens);
    }
    if (tokens.length() == 0) {
        fprintf(stderr, "%s:%d:1: unexpected EOF", script_path, line_number);
        exit(1);
    }
    if (*tokens[0].string != *new_string(RNG_DIRECTIVE))
        report_error(tokens[0], 0, "expected rng directive with tag: ", tag.raw());
    if (tokens.length() != 3)
        report_error(tokens[0], 0, "expected 2 arguments");
    if (*tokens[2].string != *new_string(tag))
        report_error(tokens[2], 0, "rng tag mismatch. expected: ", tag.raw());
    return parse_int(tokens[1]);
}

static const char * const SEED = "@seed";
static void write_seed(uint32_t seed) {
    String line = new_string();
    line->format("%s %s\n", SEED, uint32_to_string(seed));
    write_line(line);
}
static const char * const TEST_MODE_HEADER = "@test";
static void write_test_mode_header() {
    String line = new_string();
    line->format("%s\n", TEST_MODE_HEADER);
    write_line(line);
}
static void read_header() {
    List<Token> tokens;
    while (tokens.length() == 0) {
        String line = read_line();
        if (line == nullptr)
            break; // and error
        tokenize_line(line, &tokens);
    }
    if (tokens.length() == 0) {
        fprintf(stderr, "%s:%d:1: unexpected EOF", script_path, line_number);
        exit(1);
    }
    if (*tokens[0].string == *new_string(SEED)) {
        if (tokens.length() != 2) {
            fprintf(stderr, "%s:%d:1: expected 1 argument", script_path, line_number);
            exit(1);
        }
        tas_seed = parse_uint32(tokens[1]);
    } else if (*tokens[0].string == *new_string(TEST_MODE_HEADER)) {
        if (tokens.length() != 1) {
            fprintf(stderr, "%s:%d:1: expected no arguments", script_path, line_number);
            exit(1);
        }
        test_mode = true;
    } else {
        fprintf(stderr, "%s:%d:1: expected swarkland header", script_path, line_number);
        exit(1);
    }
}

void set_tas_script(TasScriptMode mode, const char * file_path) {
    init_name_arrays();
    script_path = file_path;

    switch (mode) {
        case TasScriptMode_WRITE:
            script_file = fopen(script_path, "wb");
            if (script_file == nullptr) {
                fprintf(stderr, "ERROR: could not create file: %s\n", script_path);
                exit(1);
            }
            current_mode = TasScriptMode_WRITE;
            break;
        case TasScriptMode_READ:
            script_file = fopen(script_path, "rb");
            if (script_file == nullptr) {
                fprintf(stderr, "ERROR: could not read file: %s\n", script_path);
                exit(1);
            }
            current_mode = TasScriptMode_READ;
            break;
        case TasScriptMode_READ_WRITE:
            script_file = fopen(script_path, "r+b");
            if (script_file != nullptr) {
                // first read, then write
                current_mode = TasScriptMode_READ_WRITE;
            } else {
                if (errno != ENOENT) {
                    fprintf(stderr, "ERROR: could not read/create file: %s\n", script_path);
                    exit(1);
                }
                // no problem. we'll just make it
                script_file = fopen(script_path, "wb");
                if (script_file == nullptr) {
                    fprintf(stderr, "ERROR: could not create file: %s\n", script_path);
                    exit(1);
                }
                current_mode = TasScriptMode_WRITE;
            }
            break;
        case TasScriptMode_IGNORE:
            current_mode = TasScriptMode_IGNORE;
            break;
    }

    switch (current_mode) {
        case TasScriptMode_READ:
        case TasScriptMode_READ_WRITE: {
            read_header();
            break;
        }
        case TasScriptMode_WRITE: {
            if (!test_mode) {
                tas_seed = get_random_seed();
                write_seed(tas_seed);
            } else {
                write_test_mode_header();
            }
            break;
        }
        case TasScriptMode_IGNORE:
            tas_seed = get_random_seed();
            break;
    }
}

static Action read_action() {
    List<Token> tokens;
    while (tokens.length() == 0) {
        String line = read_line();
        if (line == nullptr)
            return Action::undecided(); // EOF
        tokenize_line(line, &tokens);
    }
    Action action;
    action.id = parse_action_type(tokens[0]);
    switch (action.get_layout()) {
        case Action::Layout_VOID:
            if (tokens.length() != 1)
                report_error(tokens[0], 0, "expected no arguments");
            return action;
        case Action::Layout_COORD: {
            if (tokens.length() != 3)
                report_error(tokens[0], 0, "expected 2 arguments");
            action.coord() = parse_coord(tokens[1], tokens[2]);
            return action;
        }
        case Action::Layout_ITEM:
            if (tokens.length() != 2)
                report_error(tokens[0], 0, "expected 1 argument");
            action.item() = parse_uint256(tokens[1]);
            return action;
        case Action::Layout_COORD_AND_ITEM: {
            if (tokens.length() != 4)
                report_error(tokens[0], 0, "expected 3 arguments");
            action.coord_and_item().coord = parse_coord(tokens[1], tokens[2]);
            action.coord_and_item().item = parse_uint256(tokens[3]);
            return action;
        }
        case Action::Layout_WISH: {
            if (tokens.length() != 3)
                report_error(tokens[0], 0, "expected 2 arguments");
            Action::Wish & data = action.wish();
            data.thing_type = parse_thing_type(tokens[1]);
            switch (data.thing_type) {
                case ThingType_INDIVIDUAL:
                    report_error(tokens[1], 0, "can't wish for an individual");
                case ThingType_WAND:
                    data.wand_id = parse_wand_id(tokens[2]);
                    return action;
                case ThingType_POTION:
                    data.potion_id = parse_potion_id(tokens[2]);
                    return action;

                case ThingType_COUNT:
                    unreachable();
            }
            unreachable();
        }
        case Action::Layout_GENERATE_MONSTER: {
            if (tokens.length() != 5)
                report_error(tokens[0], 0, "expected 4 arguments");
            Action::GenerateMonster & data = action.generate_monster();
            data.species = parse_species_id(tokens[1]);
            data.decision_maker = parse_decision_maker(tokens[2]);
            data.location = parse_coord(tokens[3], tokens[4]);
            return action;
        }
    }
    unreachable();
}
static String action_to_string(const Action & action) {
    assert(action.id < Action::Id::COUNT);
    String result = new_string();
    String action_type_string = action_names[action.id];
    switch (action.get_layout()) {
        case Action::Layout_VOID:
            result->format("%s\n", action_type_string);
            break;
        case Action::Layout_COORD: {
            String coord_string1 = int_to_string(action.coord().x);
            String coord_string2 = int_to_string(action.coord().y);
            result->format("%s %s %s\n", action_type_string, coord_string1, coord_string2);
            break;
        }
        case Action::Layout_ITEM:
            result->format("%s %s\n", action_type_string, uint256_to_string(action.item()));
            break;
        case Action::Layout_COORD_AND_ITEM: {
            String coord_string1 = int_to_string(action.coord_and_item().coord.x);
            String coord_string2 = int_to_string(action.coord_and_item().coord.y);
            String item_string = uint256_to_string(action.coord_and_item().item);
            result->format("%s %s %s %s\n", action_type_string, coord_string1, coord_string2, item_string);
            break;
        }
        case Action::Layout_WISH: {
            const Action::Wish & data = action.wish();
            String thing_type_string = thing_type_names[data.thing_type];
            switch (data.thing_type) {
                case ThingType_INDIVIDUAL:
                    unreachable();
                case ThingType_WAND: {
                    String wand_id_string = wand_id_names[data.wand_id];
                    result->format("%s %s %s\n", action_type_string, thing_type_string, wand_id_string);
                    break;
                }
                case ThingType_POTION: {
                    String potion_id_string = potion_id_names[data.potion_id];
                    result->format("%s %s %s\n", action_type_string, thing_type_string, potion_id_string);
                    break;
                }

                case ThingType_COUNT:
                    unreachable();
            }
            break;
        }
        case Action::Layout_GENERATE_MONSTER: {
            const Action::GenerateMonster & data = action.generate_monster();
            String species_string = species_names[data.species];
            String decision_maker_string = decision_maker_names[data.decision_maker];
            String location_string1 = int_to_string(data.location.x);
            String location_string2 = int_to_string(data.location.y);
            result->format("%s %s %s %s %s\n", action_type_string, species_string, decision_maker_string, location_string1, location_string2);
            break;
        }
    }
    return result;
}

Action tas_get_decision() {
    if (!headless_mode && tas_delay > 0) {
        if (frame_counter < tas_delay) {
            frame_counter++;
            return Action::undecided(); // let the screen draw
        }
        frame_counter = 0;
    }
    switch (current_mode) {
        case TasScriptMode_READ_WRITE: {
            Action result = read_action();
            if (result == Action::undecided()) {
                // end of file
                current_mode = TasScriptMode_WRITE;
            }
            return result;
        }
        case TasScriptMode_READ: {
            Action result = read_action();
            if (result == Action::undecided()) {
                // end of file
                fclose(script_file);
                current_mode = TasScriptMode_IGNORE;
            }
            return result;
        }
        case TasScriptMode_WRITE:
        case TasScriptMode_IGNORE:
            // no, you decide.
            return Action::undecided();
    }
    unreachable();
}

void tas_record_decision(const Action & action) {
    switch (current_mode) {
        case TasScriptMode_READ_WRITE:
        case TasScriptMode_READ:
        case TasScriptMode_IGNORE:
            // don't write
            break;
        case TasScriptMode_WRITE:
            write_line(action_to_string(action));
            break;
    }
}
int tas_get_rng_input(const ByteBuffer & tag) {
    int value;

    switch (current_mode) {
        case TasScriptMode_READ_WRITE:
        case TasScriptMode_READ:
            // read from script
            value = read_rng_input(tag);
            break;
        case TasScriptMode_IGNORE:
        case TasScriptMode_WRITE:
            // read from stdio
            printf("%s\n", tag.raw());
            if (scanf("%d", &value) != 1)
                panic("scanf did not return 1");
            break;
    }

    switch (current_mode) {
        case TasScriptMode_READ_WRITE:
        case TasScriptMode_READ:
        case TasScriptMode_IGNORE:
            // don't write
            break;
        case TasScriptMode_WRITE: {
            write_line(rng_input_to_string(tag, value));
            break;
        }
    }

    return value;
}


void tas_delete_save() {
    switch (current_mode) {
        case TasScriptMode_READ_WRITE:
        case TasScriptMode_READ:
        case TasScriptMode_IGNORE:
            // don't delete it
            break;
        case TasScriptMode_WRITE:
            fclose(script_file);
            remove(script_path);
            current_mode = TasScriptMode_IGNORE;
            break;
    }
}
