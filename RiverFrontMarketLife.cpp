// ============================================================================
//  Scenario4_RiverfrontMarketLife.cpp  --  Scenario 4, extracted to run alone
// ----------------------------------------------------------------------------
//  This is CityLife.cpp with Scenarios 1, 2 and 3 lifted out, so the Winter
//  Night Market runs on its own. Everything inside `namespace Scenario4` is
//  byte-for-byte identical to the group file -- same functions in the same
//  order, same comments -- and so is the SHARED APP STATE block above it, so
//  anything fixed or improved here can be pasted straight back into
//  CityLife.cpp (and vice versa) with no reconciliation work.
//
//  What is different from the group file, and nothing else:
//    * the Scenario 1 / 2 / 3 namespaces are gone
//    * currentScreen starts on SCENARIO_4 and NUM_SCENARIOS is 1
//    * the display / keyboard / mouse dispatchers and main() only know
//      about Scenario4
//    * one typo fix: BeginDepthSprite() had `1..0f` where it meant `1.0f`,
//      which stops the group file compiling at all (see the note there)
//
//  The AppScreen enum still lists all four names. It is left whole on
//  purpose: Scenario4's timers each test `currentScreen != SCENARIO_4`, and
//  keeping the enum intact means not one line inside the namespace had to
//  be touched.
//
//  Scenario 4 : Riverfront Market Life - riverside market, ferris stalls,
//                                        boats on the river, aurora,
//                                        a side street running back into the
//                                        town, and birds crossing the sky
//
//  CONTROLS
//  --------
//    SPACE ... pause / resume
//    H ....... show / hide the on-screen control panel
//    X ....... force the sled, the fireworks and the aurora (demo helper)
//    1 2 3 ... snow amount: light / medium / heavy
//    4 5 ..... daytime / night (5 also resets every other toggle)
//    S ....... season: winter <-> autumn
//    L ....... warm white <-> multicolour string lights
//    F ....... snow squall
//    ESC ..... quit
//
//  SEASONS
//    'S' crossfades the whole plaza between deep winter and late autumn over
//    about three seconds. It is not a palette filter: the ice rink dissolves
//    into a working fountain, the snowman family into planters and a pumpkin
//    stack, Santa into a chestnut roaster, the Christmas tree turns and hangs
//    paper lanterns instead of baubles, every settled snow cap melts, the
//    falling snow becomes falling leaves and the river loses its winter blue.
//    Autumn rather than summer, so the coats, the braziers and the night
//    market itself all still make sense.
//
//    N / B and F1-F4 are still bound, because the shared HUD and the
//    scenario's own help panel both mention them and neither was edited.
//    With one scenario in the running order they wrap straight back onto
//    this scene, so all they do now is replay the title card.
//
//  ############################################################################
//  #  BUILD REQUIREMENT -- READ THIS BEFORE COMPILING                         #
//  #                                                                          #
//  #  This file needs C++11. Code::Blocks defaults to the 1998 standard, and  #
//  #  if you build without changing that you get a wall of errors like:       #
//  #                                                                          #
//  #      error: 'constexpr' does not name a type                             #
//  #      error: 'nullptr' was not declared in this scope                     #
//  #      error: 'MAX_DROPS' was not declared in this scope                   #
//  #                                                                          #
//  #  Those are NOT bugs in the code. They all mean one thing: the C++11      #
//  #  flag is off. Turn it on:                                                #
//  #                                                                          #
//  #    Code::Blocks                                                          #
//  #      Settings -> Compiler... -> Global compiler settings                 #
//  #        -> GNU GCC Compiler -> Compiler settings -> Compiler Flags        #
//  #        -> tick "Have g++ follow the C++11 ISO C++ language standard"     #
//  #      then Build -> Rebuild  (a plain Build reuses stale object files)    #
//  #                                                                          #
//  #    If that checkbox is missing, use the "Other compiler options" tab     #
//  #    on the same dialog and type this on its own line:  -std=gnu++11       #
//  #                                                                          #
//  #    Command line                                                          #
//  #      g++ -std=gnu++11 Scenario4_WinterNightMarket.cpp -o WinterMarket \  #
//  #          -lfreeglut -lopengl32 -lglu32                                   #
//  #                                                                          #
//  #  gnu++11 is preferred over c++11 on MinGW: strict ISO mode hides some    #
//  #  C runtime declarations that the MinGW headers need.                     #
//  ############################################################################
// ============================================================================
#if defined(__APPLE__)
  #define GL_SILENCE_DEPRECATION
  #include <OpenGL/gl.h>
  #include <GLUT/glut.h>
#elif defined(_WIN32)
  #include <windows.h>
  #include <GL/gl.h>
  #include <GL/glut.h>
#else
  #include <GL/gl.h>
  #include <GL/glut.h>
#endif
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <iostream>

using namespace std;

// ============================================================================
//  SHARED APP STATE  (used by every scenario + the home menu)
// ============================================================================
// There is no home menu any more: the program opens directly on Scenario 4
// and the viewer moves between scenes with N / B / F1-F4. HOME_MENU was
// removed from this enum rather than merely left unused, so no stale code
// path can put the program back into a screen that no longer draws anything.
//
// STANDALONE BUILD: the enum still names all four scenarios even though only
// Scenario4 is compiled in. Every timer inside the namespace idles on
// `currentScreen != SCENARIO_4`, so leaving the enum whole is what lets that
// code stay untouched.
enum AppScreen { SCENARIO_1, SCENARIO_2, SCENARIO_3, SCENARIO_4 };
AppScreen currentScreen = SCENARIO_4;

// Which scenario is on screen, as a 0-based position in the running order.
// Declared here rather than beside the navigation code at the bottom because
// the shared HUD needs it to print "SCENARIO 2 / 4".
// STANDALONE BUILD: one scenario in the running order, so the HUD reads
// "SCENARIO 1 / 1".
const int NUM_SCENARIOS = 1;
int currentScenarioIndex = 0;

const int WINDOW_WIDTH  = 1080;
const int WINDOW_HEIGHT = 720;

// Live width of the drawing viewport in pixels. reshape() keeps this in step
// with the window so bitmap text stays correctly centred after a resize.
int viewportPixelWidth = WINDOW_WIDTH;

// World-coordinate box every scenario renders into (matches the ortho range
// the original coastal-city scenario already used). Teammates are free to
// call glOrtho with different numbers inside their own Draw() if they need
// a different coordinate range -- just do it at the very start of Draw()
// every frame (see Scenario1_CoastalCity::Draw() below for the pattern) so
// switching scenarios never leaves a stale projection behind.
const float WORLD_LEFT   = -60.0f, WORLD_RIGHT = 60.0f;
const float WORLD_BOTTOM = -40.0f, WORLD_TOP   = 40.0f;

// ---- Small text-rendering helpers (available to every scenario) -----------
inline void DrawText(float x, float y, void* font, const char* text) {
    glRasterPos2f(x, y);
    for (const char* c = text; *c != '\0'; ++c) {
        glutBitmapCharacter(font, *c);
    }
}

inline int TextPixelWidth(void* font, const char* text) {
    int w = 0;
    for (const char* c = text; *c != '\0'; ++c) {
        w += glutBitmapWidth(font, *c);
    }
    return w;
}

// Centers text horizontally on world-x `cx` (bitmap fonts are pixel-sized,
// so the pixel width has to be converted into world units using the ortho
// scale before it can be used to offset a raster position).
inline void DrawTextCentered(float cx, float y, void* font, const char* text) {
    int pixelWidth = TextPixelWidth(font, text);
    float worldWidth = pixelWidth * (WORLD_RIGHT - WORLD_LEFT) / (float)viewportPixelWidth;
    DrawText(cx - worldWidth * 0.5f, y, font, text);
}

// ---- Shared depth + soft-shape helpers ------------------------------------
// Every scenario draws its ground plane between the horizon (y = -6) and the
// bottom of the world box (y = -40), but until now every figure was drawn at
// exactly the same size no matter how far down the frame it stood, so the
// ground read as a flat wall. DepthScale() converts a standing y into a size
// multiplier: small and pale near the horizon, large and close at the bottom.
//
// The invariant that goes with it: anything drawn LOWER in the frame must
// also be drawn LATER, or it will be composited behind something that is
// supposed to be further away.
const float DEPTH_HORIZON_Y = -6.0f;    // where the ground starts
const float DEPTH_NEAR_Y    = -26.0f;   // "as close as the camera gets"

inline float DepthScale(float y) {
    float t = (y - DEPTH_HORIZON_Y) / (DEPTH_NEAR_Y - DEPTH_HORIZON_Y);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return 0.62f + t * (1.36f - 0.62f);
}

// Same idea with an explicit range, for scenarios whose ground band is
// shallower than the full world box (Scenario 2's road, for example).
inline float DepthScaleRange(float y, float farY, float nearY,
                             float farScale, float nearScale) {
    float t = (y - farY) / (nearY - farY);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return farScale + t * (nearScale - farScale);
}

// A stack of concentric ellipses with alpha rising toward the middle. Used
// for two different jobs: contact shadows under figures, and the pools of
// light that lamps, fires and neon throw onto the ground.
inline void DrawSoftEllipse(float cx, float cy, float rx, float ry,
                            unsigned char r, unsigned char g, unsigned char b,
                            unsigned char a, int layers) {
    if (layers < 1) layers = 1;
    for (int L = layers; L >= 1; --L) {
        float f = (float)L / (float)layers;                 // 1 = outermost ring
        glColor4ub(r, g, b, (unsigned char)(a / (float)L));
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(cx, cy);
            for (int i = 0; i <= 24; i++) {
                float ang = (float)i / 24.0f * 6.2831853f;
                glVertex2f(cx + rx * f * cosf(ang), cy + ry * f * sinf(ang));
            }
        glEnd();
    }
}

// Scales a sprite about its own anchor point, so a drawing function can keep
// using its original world coordinates and still be resized for depth.
inline void BeginDepthSprite(float anchorX, float anchorY, float scale) {
    glPushMatrix();
    glTranslatef(anchorX, anchorY, 0.0f);
    glScalef(scale, scale, 1.0f);   
    glTranslatef(-anchorX, -anchorY, 0.0f);
}
inline void EndDepthSprite() { glPopMatrix(); }

// ---- Reflections -----------------------------------------------------------
//  Water is the one surface in this project that has to show what is above
//  it. The pattern is three calls:
//
//      BeginReflection(waterlineY, 0.62f, wobble);
//      DrawTheThing();                       // its own, unchanged geometry
//      EndReflection();
//      WashReflection(cx, halfWidth, waterlineY, depth, waterR,G,B, alpha,
//                     ripplePhase);
//
//  BeginReflection mirrors and squashes the drawing below the waterline;
//  WashReflection then knocks it back toward the water colour and lays
//  horizontal ripple bands across it, which is what stops a mirrored copy
//  looking like an upside-down twin and starts it looking like a reflection.
inline void BeginReflection(float surfaceY, float squash, float wobble) {
    glPushMatrix();
    glTranslatef(wobble, surfaceY, 0.0f);
    glScalef(1.0f, -squash, 1.0f);
    glTranslatef(-wobble, -surfaceY, 0.0f);
}
inline void EndReflection() { glPopMatrix(); }

inline void WashReflection(float cx, float halfW, float surfaceY, float depth,
                           unsigned char r, unsigned char g, unsigned char b,
                           unsigned char nearAlpha, float ripplePhase) {
    // Fade toward the water colour with depth.
    glBegin(GL_QUADS);
        glColor4ub(r, g, b, nearAlpha);
        glVertex2f(cx - halfW, surfaceY);
        glVertex2f(cx + halfW, surfaceY);
        glColor4ub(r, g, b, 255);
        glVertex2f(cx + halfW, surfaceY - depth);
        glVertex2f(cx - halfW, surfaceY - depth);
    glEnd();

    // Ripple bands slicing across the reflection.
    for (int i = 0; i < 7; i++) {
        float t  = (i + 0.5f) / 7.0f;
        float y  = surfaceY - t * depth;
        float th = 0.10f + 0.16f * (0.5f + 0.5f * sinf(ripplePhase * 0.7f + i * 1.9f));
        float sx = sinf(ripplePhase * 0.5f + i * 2.3f) * halfW * 0.18f;
        glColor4ub(r, g, b, (unsigned char)(150 + 80 * t));
        glBegin(GL_QUADS);
            glVertex2f(cx - halfW + sx, y);
            glVertex2f(cx + halfW + sx, y);
            glVertex2f(cx + halfW + sx, y - th);
            glVertex2f(cx - halfW + sx, y - th);
        glEnd();
    }
}

// Contact shadow: a squashed dark ellipse sitting at a figure's feet. `lean`
// slides it sideways so shadows can fall away from a scene's light source.
inline void DrawGroundShadow(float cx, float cy, float rx, float lean,
                             unsigned char a) {
    DrawSoftEllipse(cx + lean, cy, rx, rx * 0.26f, 18, 22, 30, a, 3);
}

// ---- Shared HUD / help overlay --------------------------------------------
// Every scenario has keyboard controls that were previously undiscoverable.
// Each scenario calls DrawSceneHUD() at the very end of its Draw() with its
// own title and control lines; H toggles the panel, and a compact "H help"
// hint is shown when it is hidden so the user can always find it.
bool showHelp = true;   // toggled with 'H', shared by all scenarios
bool isPaused = false;  // toggled with SPACE, shared by all scenarios

// Draws a translucent panel in the lower-left corner listing this scenario's
// controls. `lines` is a NULL-terminated array of strings.
inline void DrawSceneHUD(const char* title, const char* const* lines) {
    int count = 0;
    while (lines[count] != nullptr) count++;

    // Position in the running order, always on screen. With the home menu
    // gone this is the only thing telling the viewer where they are in the
    // sequence and how to move on, so it is drawn whether or not the help
    // panel is open.
    // sprintf rather than snprintf: old MinGW builds (the toolchain that
    // ships with Code::Blocks) do not declare snprintf under strict ISO
    // mode. The buffer is fixed and the inputs are two small integers.
    char navLine[64];
    sprintf(navLine, "SCENARIO %d / %d", currentScenarioIndex + 1, NUM_SCENARIOS);
    glColor4ub(255, 255, 255, 210);
    DrawText(WORLD_RIGHT - 15.0f, WORLD_TOP - 2.6f, GLUT_BITMAP_HELVETICA_12, navLine);
    glColor4ub(200, 212, 230, 175);
    DrawText(WORLD_RIGHT - 15.0f, WORLD_TOP - 4.4f, GLUT_BITMAP_HELVETICA_10,
             "N next    B back");

    if (!showHelp) {
        // Collapsed state: just a small hint so the panel is rediscoverable.
        glColor4ub(255, 255, 255, 130);
        DrawText(WORLD_LEFT + 1.5f, WORLD_BOTTOM + 1.4f,
                 GLUT_BITMAP_HELVETICA_10, "H  help");
        if (isPaused) {
            glColor4ub(255, 220, 120, 220);
            DrawText(WORLD_LEFT + 1.5f, WORLD_BOTTOM + 3.2f,
                     GLUT_BITMAP_HELVETICA_12, "|| PAUSED");
        }
        return;
    }

    float lineH = 2.0f;
    float panelH = 4.2f + count * lineH;
    float panelW = 34.0f;
    for (int i = 0; i < count; i++) {
        int pixelW = TextPixelWidth(GLUT_BITMAP_HELVETICA_10, lines[i]);
        float worldW = pixelW * (WORLD_RIGHT - WORLD_LEFT) / (float)viewportPixelWidth;
        if (worldW + 3.2f > panelW) panelW = worldW + 3.2f;
    }
    float x0 = WORLD_LEFT + 1.0f;
    float y0 = WORLD_BOTTOM + 1.0f;

    // Backing panel (kept dark and translucent so it never fights the scene)
    glColor4ub(10, 12, 18, 165);
    glBegin(GL_QUADS);
        glVertex2f(x0, y0);
        glVertex2f(x0 + panelW, y0);
        glVertex2f(x0 + panelW, y0 + panelH);
        glVertex2f(x0, y0 + panelH);
    glEnd();
    glColor4ub(180, 190, 210, 110);
    glLineWidth(1.0f);
    glBegin(GL_LINE_LOOP);
        glVertex2f(x0, y0);
        glVertex2f(x0 + panelW, y0);
        glVertex2f(x0 + panelW, y0 + panelH);
        glVertex2f(x0, y0 + panelH);
    glEnd();

    float ty = y0 + panelH - 2.3f;
    glColor4ub(255, 255, 255, 235);
    DrawText(x0 + 1.2f, ty, GLUT_BITMAP_HELVETICA_12, title);
    ty -= 1.0f;

    if (isPaused) {
        glColor4ub(255, 220, 120, 235);
        DrawText(x0 + panelW - 9.5f, y0 + panelH - 2.3f,
                 GLUT_BITMAP_HELVETICA_12, "|| PAUSED");
    }

    glColor4ub(200, 210, 225, 210);
    for (int i = 0; i < count; i++) {
        ty -= lineH;
        DrawText(x0 + 1.2f, ty, GLUT_BITMAP_HELVETICA_10, lines[i]);
    }
    glLineWidth(1.0f);
}

// ============================================================================
//  SCENARIO 4  --  placeholder for a teammate's scenario
// ----------------------------------------------------------------------------
//  Replace the TODOs below with your own scene. See TEAMMATE_GUIDE.md for a
//  step-by-step walkthrough. The program already compiles and runs with this
//  placeholder in place, so you can build/run at any time while you work.
// ============================================================================
namespace Scenario4 {

// ============================================================================
//  STATE
// ============================================================================
constexpr float PI4 = 3.1416f;

bool  isAnimating4      = true;
int   snowIntensity4    = 1;     // 0 = light, 1 = medium, 2 = heavy (keys 1/2/3)
bool  multicolorLights4 = false; // toggled with 'L'

// ---- Time of day -----------------------------------------------------------
// The scene has one master parameter for daylight: 0 is the default night,
// 1 is full day. Key 4 raises it, key 5 drops it, and dayT4 eases toward the
// target a step per tick so the change plays as a sunrise or a sunset instead
// of a cut -- which also means every value derived from it can be a plain
// linear blend and will animate for free.
//
// Two rules keep the rest of the file consistent:
//   * anything made of MATTER (walls, ice, stone, cloth) picks its colour with
//     ColourDN4 / ColourDN4A, which crossfades a night palette entry into a
//     day one;
//   * anything that is LIGHT (bulbs, windows, halos, light pools, the glow on
//     the snow) multiplies its alpha or brightness by NightT4, so every
//     artificial source fades out as the sun comes up and no daylight frame
//     contains a glow that the sun would have washed away.
float dayT4      = 0.0f;
float dayTarget4 = 0.0f;

inline float MixF4(float night, float day) { return night + (day - night) * dayT4; }

inline unsigned char MixB4(int night, int day) {
    float v = night + (day - night) * dayT4;
    if (v < 0.0f)   v = 0.0f;
    if (v > 255.0f) v = 255.0f;
    return (unsigned char)(v + 0.5f);
}

inline unsigned char DayGlow4(int night, int day, float strength = 1.0f) {
    return (unsigned char)((night + (day - night) * dayT4) * strength + 0.5f);
}

// Set the current colour from a night/day pair.
inline void ColourDN4(int nr, int ng, int nb, int dr, int dg, int db) {
    glColor3ub(MixB4(nr, dr), MixB4(ng, dg), MixB4(nb, db));
}
inline void ColourDN4A(int nr, int ng, int nb, int na,
                       int dr, int dg, int db, int da) {
    glColor4ub(MixB4(nr, dr), MixB4(ng, dg), MixB4(nb, db), MixB4(na, da));
}

// How much of the scene's artificial lighting still reads: 1 at night, 0 by day.
inline float NightT4() { return 1.0f - dayT4; }
inline unsigned char NightA4(int nightAlpha) {
    return (unsigned char)(nightAlpha * NightT4());
}
// Dims a lit colour channel toward an unlit daylight value.
inline unsigned char LampB4(int lit, int unlit) {
    return (unsigned char)(unlit + (lit - unlit) * NightT4());
}

// ============================================================================
//  SEASON  --  winter <-> autumn, toggled with 'S'
// ----------------------------------------------------------------------------
//  Built on exactly the same pattern as the day/night mix above: one value
//  eases between 0 and 1 and every colour in the scene is written as a pair.
//  Nothing snaps; the whole plaza crossfades over about three seconds.
//
//      seasonT4 = 0   deep winter -- snow, ice, the market in full swing
//      seasonT4 = 1   late autumn -- wet ochre paving, fallen leaves, open
//                     water, the same market dressed for a different month
//
//  Autumn was chosen over summer deliberately. It is still cold enough that
//  the coats, the braziers, the string lights and the night market itself all
//  still make sense -- a summer version would have left an ice rink, a
//  snowman family and Santa standing in the sun with no explanation.
float seasonT4      = 0.0f;
float seasonTarget4 = 0.0f;

// Winter <-> autumn mixers, mirroring MixF4 / MixB4 / ColourDN4.
inline float MixSF4(float winter, float autumnV) {
    return winter + (autumnV - winter) * seasonT4;
}
inline unsigned char MixSB4(int winter, int autumnV) {
    float v = winter + (autumnV - winter) * seasonT4;
    if (v < 0.0f)   v = 0.0f;
    if (v > 255.0f) v = 255.0f;
    return (unsigned char)(v + 0.5f);
}
inline void ColourSeason4(int wr, int wg, int wb, int ar, int ag, int ab) {
    glColor3ub(MixSB4(wr, ar), MixSB4(wg, ag), MixSB4(wb, ab));
}

// How much of winter still reads: 1 in winter, 0 in autumn. Anything made of
// snow or ice fades out through this, so nothing has to be branched on.
inline float SeasonWinter4() { return 1.0f - seasonT4; }
inline float SeasonAutumn4() { return seasonT4; }

// Props that only belong to one season fade rather than vanish, so the
// crossfade never shows an object popping out of existence.
inline unsigned char WinterA4(int alpha) {
    return (unsigned char)(alpha * SeasonWinter4());
}
inline unsigned char AutumnA4(int alpha) {
    return (unsigned char)(alpha * SeasonAutumn4());
}

// Settled snow depth. Declared here rather than beside UpdateSnow4 because the
// season helpers below need it; SnowDepth4() zeroes it out of season, so roof
// caps and drifts melt away as autumn comes in.
float snowCover4 = 0.55f;

inline float SnowDepth4() { return snowCover4 * SeasonWinter4(); }
float twinklePhase4     = 0.0f;
float pedTimer4         = 0.0f;

// ---- Stars -----------------------------------------------------------
// A real winter sky is mostly faint pinpricks with a handful of bright ones,
// so every star carries its own size, brightness, colour temperature and
// twinkle rate. Drawing the whole field at one size and one alpha is what
// made the old 25-star scatter read as a row of identical dots.
constexpr int NUM_STARS4 = 190;
struct Star4 { float x, y, twinkle, mag, rate; int bucket; unsigned char r, g, b; };
Star4 stars4[NUM_STARS4];

// The few stars bright enough to earn a halo and a cross flare. Without them
// a star field has no focal points and reads as uniform noise.
constexpr int NUM_BRIGHT_STARS4 = 8;
struct BrightStar4 { float x, y, r, phase; };
BrightStar4 brightStars4[NUM_BRIGHT_STARS4];

// ---- Flying birds -----------------------------------------------------
// Small birds crossing the distant sky. Each one gets its own speed, height,
// wing rate and vertical drift, so the group never moves as one rigid block
// -- which is the single thing that makes flocking animation look fake.
constexpr int NUM_BIRDS4 = 12;
struct Bird4 {
    float x, y, speed, scale;
    float flapPhase, flapRate;   // wing beat
    float bobPhase,  bobAmp;     // slow rise and fall along the flight path
    int   dir;                   // +1 flying right, -1 flying left
};
Bird4 birds4[NUM_BIRDS4];

// ---- Shop backdrop -----------------------------------------------------
// x is the centre of the wall, w its width, h its height above the plaza
// horizon at y = -6. The roof overhangs the wall by SHOP_EAVE_OVER4 at each
// end, so two shops only read as separate buildings if their ROOF spans clear
// each other -- that is the figure this layout is spaced on, not w.
//
//   shop      wall span          roof span          gap to the next roof
//   (corner)  -61.0 .. -49.0     -61.6 .. -48.4     19.4  (ferris wheel)
//   TOYS      -28.4 .. -17.0     -29.0 .. -16.4      2.3
//   BAKERY    -13.5 ..  -0.9     -14.1 ..  -0.3      2.3
//   GIFTS       2.6 ..  15.8       2.0 ..  16.4      2.2
//   CAFE       19.2 ..  31.2      18.6 ..  31.8      2.4
//   SKATES     34.8 ..  45.6      34.2 ..  46.2      3.2  (clock tower)
//   CIDER      50.0 ..  62.0      49.4 ..  62.6      --   (runs off frame)
//
// Two things in front of the row would otherwise sit on a name board, so the
// named shops are kept out of their reach entirely rather than nudged to a
// margin that one frame of animation can close:
//
//   * the ferris wheel sweeps x [-54.5, -29.5], and a cabin crossing a board
//     hides most of it for well over half of every revolution;
//   * the street lamp at x = -58 is solid up to y = 3.2, which is exactly the
//     height a board over an awning wants.
//
// Between them they rule out a signed shop anywhere left of about -29, so the
// far-left corner gets an unnamed end-of-terrace instead: it keeps the
// skyline from stopping dead at the wheel, and having no board there is
// nothing to obscure. The clock tower (x 47.2 .. 52.8) stands in the wide gap
// on the right, in front of CIDER's blank left end and clear of both boards.
struct Shop4 { float x, w, h; const char* sign; bool lit[5]; };
constexpr int NUM_SHOPS4 = 7;
Shop4 shops4[NUM_SHOPS4] = {
    { -22.7f, 11.4f, 17.0f, "TOYS",   {} },
    {  -7.2f, 12.6f, 21.0f, "BAKERY", {} },
    {   9.2f, 13.2f, 18.0f, "GIFTS",  {} },
    {  25.2f, 12.0f, 22.0f, "CAFE",   {} },
    {  40.2f, 10.8f, 19.0f, "SKATES", {} },
    {  56.0f, 12.0f, 18.0f, "CIDER",  {} },
    { -55.0f, 12.0f, 17.0f, nullptr,  {} }   // behind the wheel: no board
};

// ---- String lights ---------------------------------------------------
constexpr int NUM_LIGHT_SPANS4 = 5;
constexpr int BULBS_PER_SPAN4  = 8;
struct LightSpan4 { float x1, y1, x2, y2; };
// One continuous garland: each span ends where the next begins, and every
// endpoint is tied just under the eaves of the shop it belongs to, so the run
// steps up and down with the roofline instead of cutting across it.
LightSpan4 lightSpans4[NUM_LIGHT_SPANS4] = {
    { -28.4f, 10.0f, -13.5f, 14.0f },   // TOYS   -> BAKERY
    { -13.5f, 14.0f,   2.6f, 11.0f },   // BAKERY -> GIFTS
    {   2.6f, 11.0f,  19.2f, 15.0f },   // GIFTS  -> CAFE
    {  19.2f, 15.0f,  34.8f, 12.0f },   // CAFE   -> SKATES
    {  34.8f, 12.0f,  50.0f, 11.0f }    // SKATES -> CIDER
};
bool bulbLit4[NUM_LIGHT_SPANS4][BULBS_PER_SPAN4];

// ---- Fixed scene anchors --------------------------------------------------
// Declared up here (rather than beside their draw functions) because the
// lighting pass needs them before those functions appear.
constexpr float treeX4  =  0.0f;    // Christmas tree centrepiece
constexpr float clockX4 = 50.0f;    // clock tower

// ---- Market stalls ---------------------------------------------------------
// The stalls stand on the open plaza in front of the shop row, which makes
// them the nearest built thing in the scene -- so they have to read as bigger
// than the shops behind them, not smaller. stallScale4 sizes the whole stall
// from its feet up; at 1.0 the canopy came out 3.6 tall, exactly the height of
// a pedestrian, which is what made them look like toys.
//
// The x positions are spaced on the canopy width (2*StallHalfW4() = 8.06) so
// no two stalls touch, and the pairs stay clear of what shares the plaza with
// them: the ferris wheel reaches x = -30.8 at canopy height, the snowmen end
// at 22.6 and the nutcracker stands at -9.
constexpr int   NUM_STALLS4   = 4;
constexpr float stallScale4   = 1.55f;
// Dropped from -11.5: the canopy apex sits 3.6 * stallScale4 above this, which
// put it at y = -5.92 -- just over the shop row's base line at y = -6, so every
// stall roof cut a notch into the shopfront behind it.
constexpr float stallGroundY4 = -12.1f;   // where the stall meets the snow

// Canopy geometry, in world units, derived from the scale so the snow cap,
// the light pool and the name board all follow when stallScale4 changes.
inline float StallHalfW4()       { return 2.60f * stallScale4; }
inline float StallCounterTopY4() { return stallGroundY4 + 2.0f * stallScale4; }
inline float StallApexY4()       { return stallGroundY4 + 3.6f * stallScale4; }

struct Stall4 { float x; unsigned char r, g, b; const char* sign; int goods; };
Stall4 stalls4[NUM_STALLS4] = {
    // COCOA used to be held at -24 to keep its name board out of the swing of
    // a ferris-wheel cabin. The wheel has since moved to x = -52, so nothing
    // reaches this far right any more and the position is now free.
    { -24.0f, 130,  80, 170, "COCOA",     0 },
    { -15.0f, 200,  60,  60, "WREATHS",   1 },
    {  28.0f,  60, 140,  90, "CHESTNUTS", 2 },
    // Renamed from TOYS: now that every board is legible, it read as a
    // duplicate of the TOYS shop on the other side of the plaza.
    {  38.0f, 210, 160,  60, "TRINKETS",  3 }
};

// ---- Ferris wheel -- the unique detail for this scenario ------------------
float ferrisAngle4 = 0.0f;
// Front-left of the plaza, standing on the near paving just behind the main
// road. The ground line is the fixed anchor and the hub is derived from the
// radius, so changing ferrisR4 alone resizes the ride without lifting it off
// the snow or burying its feet.
// Moved well left and pushed BACK up the plaza.
//
//  The wheel used to stand at x = -28 with a 12.5 radius and its feet on
//  y = -16.5. That put it squarely over the TOYS shopfront (x -28.7 .. -16.7),
//  hiding a whole shop -- and it could not simply slide left, because at foot
//  height the road spans x -54.8 .. -38.0 and the wheel would have been
//  standing in the middle of the street.
//
//  The fix is to move it left AND back: at y = -10 the road has narrowed to
//  x -46.4 .. -35.3, so a wheel centred on -52 puts its right foot at about
//  -46.8, clear of the kerb. Standing further back also makes it smaller and
//  hazier, which is correct for the extra distance, and the only shop now
//  behind it is shops4[6] -- the one deliberately left without a name board
//  precisely because it sits behind the ride.
constexpr float ferrisCX4    = -52.0f;
constexpr float ferrisR4     =  10.5f;
constexpr float ferrisBaseY4 = -10.0f;                          // A-frame feet on the snow
constexpr float ferrisCY4    = ferrisBaseY4 + ferrisR4 + 1.2f;  // hub: lowest cabin clears the ground
// Cabins, bulbs, hub lamp and frame are all drawn at this scale, so the ride
// keeps its proportions at any radius. 9.0 is the radius the parts below were
// originally drawn against.
constexpr float ferrisScale4 = ferrisR4 / 9.0f;
constexpr int   NUM_CABINS4 = 8;

// ---- Ice rink + skaters ------------------------------------------------
constexpr float rinkCX4 = 10.0f, rinkCY4 = -14.5f;
struct Skater4 { float angle, speed, radiusX, radiusY; unsigned char r, g, b; };
// Five skaters on deliberately different radii and speeds so they read as
// individuals rather than one ellipse: beginners hug the outside slowly,
// the fast skater takes a tight inside line.
constexpr int NUM_SKATERS4 = 5;
// Skaters 3 and 4 deliberately share a radius and a speed with a small angle
// offset between them, so they travel side by side and can hold hands -- five
// independent ellipses never meet, and a rink where nobody interacts reads as
// five separate animations rather than one crowd.
Skater4 skaters4[NUM_SKATERS4] = {
    { 0.0f,  0.013f, 5.1f, 1.95f, 210, 70,  90  },  // beginner, wide + slow
    { 3.14f, 0.017f, 4.2f, 1.60f,  70, 110, 200 },  // the one who falls over
    { 1.6f,  0.030f, 2.4f, 0.95f, 240, 200,  60 },  // fast, tight line
    { 4.4f,  0.019f, 3.6f, 1.38f, 120, 200, 140 },  // pair, inside partner
    { 4.68f, 0.019f, 3.6f, 1.38f, 200, 130, 220 }   // pair, outside partner
};

// The blue skater takes a tumble every so often: 0 = upright, then a short
// fall, a beat sitting on the ice, and a scramble back up.
float skaterFallT4     = 0.0f;
int   skaterFallCool4  = 420;
constexpr int FALLING_SKATER4 = 1;

// ---- Horse-drawn sled --------------------------------------------------
float sledX4        = -80.0f;
bool  sledActive4    = false;
int   sledCooldown4  = 300;

// ---- Fire pit ------------------------------------------------------
constexpr float fireX4 = -5.0f, fireY4 = -10.5f;
float firePhase4 = 0.0f;

// ---- Chimney smoke / stall steam (shared particle type) --------------
struct SmokePuff4 { float x, y, vy, alpha, size; bool active; };
constexpr int MAX_SMOKE4 = 30;
SmokePuff4 chimneySmoke4[MAX_SMOKE4];
constexpr int MAX_STEAM4 = 30;
SmokePuff4 stallSteam4[MAX_STEAM4];

// ---- Snow particles ----------------------------------------------------
constexpr int MAX_SNOW4 = 300;
float snowX4[MAX_SNOW4], snowY4[MAX_SNOW4], snowDrift4[MAX_SNOW4], snowSize4[MAX_SNOW4];

// ---- Pedestrians -----------------------------------------------------
struct Ped4 { float x, y, speed, phase; int dir; unsigned char coatR, coatG, coatB; };
constexpr int NUM_PEDS4 = 7;
// Spread across the depth of the plaza rather than standing on one line.
Ped4 peds4[NUM_PEDS4] = {
    { -30.0f, -10.4f, 0.05f, 0.0f,  1, 150,  40,  50 },
    {  -5.0f, -13.6f, 0.07f, 1.0f, -1,  40,  60, 120 },
    {  20.0f, -10.8f, 0.04f, 2.0f,  1,  90,  90,  90 },
    {  35.0f, -12.4f, 0.06f, 3.0f, -1, 120,  70,  40 },
    // Was y = -10.2: at that depth the hat tip reached y = -7.38, half a
    // whisker above the shopfront sills at -7.4, so this walker clipped the
    // bottom of every front it passed.
    { -45.0f, -10.9f, 0.05f, 4.0f,  1, 200, 170,  60 },
    {  10.0f, -13.9f, 0.055f,5.0f,  1, 180,  60, 130 },
    { -18.0f, -12.9f, 0.065f,6.0f, -1,  60, 130, 110 }
};

// ---- Flying sleigh -- the signature feature for this scenario -------------
float sleighX4          = -100.0f;
float sleighY4          = 27.0f;
// Was 0.6, which put the whole team and sleigh inside about six world units --
// a smudge crossing a very large sky. At 1.05 it is roughly eleven units nose
// to tail, big enough to read as Santa at a glance without dominating the
// frame or dropping into the shop roofline.
float sleighScale4      = 1.05f;
float sleighTrailPhase4 = 0.0f;

// ---- Metro rail animation state -----------------------------------------
// `metroCarX4` is the horizontal centre of the passing carriage. The rail
// will trigger after `metroNextStart4` seconds and then move across the
// scene; it repeats on a longer interval after the first pass.
float metroCarX4      = -200.0f;
float metroTime4      = 0.0f;
float metroNextStart4 = 5.0f;   // first pass at 5 seconds
bool  metroActive4    = false;
float metroSpeed4     = 1.8f;   // world units per update tick

// ---- Clock tower pendulum ------------------------------------------------
float pendulumAngle4 = 0.0f;

// ---- Fireworks -----------------------------------------------------------
// Staged: a rocket climbs, hangs for a beat, then bursts. Previously bursts
// simply appeared in empty sky with no launch.
enum FireworkState4 { FW_INACTIVE, FW_RISING, FW_BURSTING };
struct Firework4 {
    float x, y;            // current position
    float targetY;         // apex where it bursts
    float phase;           // 0..1 burst progress
    FireworkState4 state;
    unsigned char r, g, b;
    float sparkAng[18];    // per-spark direction, so bursts differ
    float sparkSpd[18];
};
constexpr int MAX_FIREWORKS4 = 3;
Firework4 fireworks4[MAX_FIREWORKS4];
int fireworkCooldown4 = 200;

// ---- Snow accumulation ---------------------------------------------------
// Eases toward a target set by the 1/2/3 keys, so settled snow thickens or
// melts back gradually instead of snapping. (snowCover4 itself is declared up
// with the season helpers, which need it.)

inline float SnowCoverTarget4() {
    if (snowIntensity4 == 0) return 0.20f;
    if (snowIntensity4 == 1) return 0.55f;
    return 0.90f;
}

// ---- Fog / snow-squall mode (toggled with 'F') ----------------------------
// A drifting translucent band that dims whatever is behind it and makes every
// ground glow bloom. It also gives the aurora something to contrast against.
bool  fogMode4  = false;
float fogPhase4 = 0.0f;
float fogAmount4 = 0.0f;      // eases in and out so the toggle is not a jump

// ---- Aurora / northern lights (rare sky effect) ---------------------------
bool  auroraActive4   = false;
float auroraPhase4    = 0.0f;
float auroraTimer4    = 0.0f;
int   auroraCooldown4 = 600;

// ============================================================================
//  SMALL HELPERS
// ============================================================================
void FilledCircle4(float xc, float yc, float radius, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    glColor4ub(r, g, b, a);
    glBegin(GL_POLYGON);
    const int seg = 20;
    for (int i = 0; i < seg; i++) {
        float ang = (float)i / seg * 2.0f * PI4;
        glVertex2f(xc + radius * cos(ang), yc + radius * sin(ang));
    }
    glEnd();
}

// ============================================================================
//  SKY / STARS / SHOPS / STRING LIGHTS
// ============================================================================
// A single place to ask "what colour is the air at height y right now?".
// The distant buildings, the road haze and the atmospheric fade on the shop
// row all wash toward this, so every layer of the scene agrees on where the
// horizon sits and how thick the air in front of it is.
inline void SkyAirColour4(float y, float& r, float& g, float& b) {
    float t = (y + 6.0f) / 46.0f;            // 0 at the horizon, 1 at the zenith
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    r = MixF4(44.0f - 30.0f * t, 216.0f - 150.0f * t);
    g = MixF4(50.0f - 34.0f * t, 230.0f - 100.0f * t);
    b = MixF4(80.0f - 52.0f * t, 244.0f -  30.0f * t);
}

// Blends a surface colour toward the air in front of it. `depth` is 0 for
// something at arm's length and 1 for something on the horizon: the further
// back a mass stands, the more air is between it and the camera, so it loses
// contrast and saturation rather than simply getting darker. This is the one
// cue that separates the three building rows from each other.
inline void AirFade4(float depth, float atY,
                     int nr, int ng, int nb, int dr, int dg, int db,
                     unsigned char& outR, unsigned char& outG, unsigned char& outB) {
    float hr, hg, hb;
    SkyAirColour4(atY, hr, hg, hb);
    float br = MixF4((float)nr, (float)dr);
    float bg = MixF4((float)ng, (float)dg);
    float bb = MixF4((float)nb, (float)db);
    float k = depth; if (k < 0.0f) k = 0.0f; if (k > 1.0f) k = 1.0f;
    outR = (unsigned char)(br + (hr - br) * k + 0.5f);
    outG = (unsigned char)(bg + (hg - bg) * k + 0.5f);
    outB = (unsigned char)(bb + (hb - bb) * k + 0.5f);
}

// A smooth radial falloff, as one triangle fan with a bright centre and a
// fully transparent rim. DrawSoftEllipse() stacks 24-sided polygons, which is
// invisible at the size of a footprint shadow and glaringly obvious at the
// size of a moon halo -- this is the version for anything large.
inline void DrawRadialGlow4(float cx, float cy, float rx, float ry,
                            unsigned char r, unsigned char g, unsigned char b,
                            unsigned char centreA) {
    const int SEG = 48;
    glBegin(GL_TRIANGLE_FAN);
        glColor4ub(r, g, b, centreA);
        glVertex2f(cx, cy);
        glColor4ub(r, g, b, 0);
        for (int i = 0; i <= SEG; i++) {
            float a = (float)i / SEG * 6.2831853f;
            glVertex2f(cx + rx * cosf(a), cy + ry * sinf(a));
        }
    glEnd();
}

// One drifting bank of haze: a horizontal band whose alpha rises to a peak in
// the middle and falls to nothing at both ends and both edges, so it has no
// boundary anywhere. Built from quad strips rather than stacked ellipses for
// exactly the same reason DrawRadialGlow4 exists.
inline void DrawHazeBank4(float cx, float cy, float rx, float ry,
                          unsigned char r, unsigned char g, unsigned char b,
                          float peakA) {
    const int COLS = 26;
    for (int half = 0; half < 2; half++) {
        float edgeY = (half == 0) ? cy + ry : cy - ry;
        glBegin(GL_QUAD_STRIP);
        for (int k = 0; k <= COLS; k++) {
            float u    = (float)k / COLS;
            float x    = cx - rx + 2.0f * rx * u;
            float bell = 0.5f - 0.5f * cosf(u * 6.2831853f);
            bell *= bell;
            glColor4ub(r, g, b, (unsigned char)(peakA * bell));
            glVertex2f(x, cy);
            glColor4ub(r, g, b, 0);
            glVertex2f(x, edgeY);
        }
        glEnd();
    }
}

void DrawSky4() {
    float dayBlend = dayT4;

    // ---- Vertical gradient -------------------------------------------------
    // Five stops rather than two. A real sky loses most of its depth in the
    // top third and then flattens out toward the horizon, which a single
    // linear ramp between two colours cannot do -- that flat ramp is what
    // made the old backdrop read as a painted board. Every stop is a
    // night/day pair, so the whole gradient crossfades with the 4 / 5 keys.
    const int   NSTOP = 6;
    const float stopY [NSTOP]    = { 40.0f, 30.0f, 21.0f, 13.0f, 4.0f, -6.0f };
    // Two sets of stops per time of day, one per season: winter skies stay
    // blue and hard, autumn skies go warm and dusty, and both crossfade with
    // the 4 / 5 keys exactly as before.
    const int wNightC[NSTOP][3] = { {4,6,18}, {7,10,27}, {12,17,38},
                                    {19,25,52}, {29,36,68}, {44,50,82} };
    const int aNightC[NSTOP][3] = { {12,8,18}, {20,13,26}, {32,20,34},
                                    {48,29,42}, {68,41,48}, {92,58,54} };
    const int wDayC  [NSTOP][3] = { {56,120,208}, {74,140,220}, {100,164,232},
                                    {136,190,240}, {180,213,246}, {216,231,245} };
    const int aDayC  [NSTOP][3] = { {84,118,168}, {112,140,178}, {148,166,186},
                                    {186,188,188}, {218,204,180}, {236,218,184} };

    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i < NSTOP; i++) {
        glColor3ub(MixB4(MixSB4(wNightC[i][0], aNightC[i][0]),
                         MixSB4(wDayC  [i][0], aDayC  [i][0])),
                   MixB4(MixSB4(wNightC[i][1], aNightC[i][1]),
                         MixSB4(wDayC  [i][1], aDayC  [i][1])),
                   MixB4(MixSB4(wNightC[i][2], aNightC[i][2]),
                         MixSB4(wDayC  [i][2], aDayC  [i][2])));
        glVertex2f(-60.0f, stopY[i]);
        glVertex2f( 60.0f, stopY[i]);
    }
    glEnd();

    // ---- Sideways variation -----------------------------------------------
    // A sky that is identical at every x still reads as wallpaper. The light
    // source sits up and to the right (moon by night, sun by day), so the air
    // on that side is a touch brighter and warmer.
    glBegin(GL_QUADS);
        glColor4ub(MixB4(70, 255), MixB4(86, 240), MixB4(126, 214), MixB4(30, 46));
        glVertex2f(60.0f, 40.0f); glVertex2f(60.0f, -6.0f);
        glColor4ub(MixB4(70, 255), MixB4(86, 240), MixB4(126, 214), 0);
        glVertex2f(-6.0f, -6.0f); glVertex2f(-6.0f, 40.0f);
    glEnd();

    // ---- Haze banks --------------------------------------------------------
    // Long, very soft horizontal smears drifting at different rates. They are
    // deliberately far too faint to read as "clouds"; their job is to break up
    // the flat gradient so the eye finds something to settle on.
    for (int i = 0; i < 6; i++) {
        float span  = 260.0f;
        float drift = fmodf(twinklePhase4 * (0.22f + i * 0.09f) + i * 43.0f, span) - span * 0.5f;
        float cy    = 33.0f - i * 5.4f;
        float rx    = 24.0f + i * 7.0f;
        float ry    = 1.6f  + i * 0.7f;
        float a     = MixB4(20, 40) * (1.0f - i * 0.09f);
        unsigned char cr = MixB4(92, 244), cg = MixB4(108, 248), cb = MixB4(148, 252);
        // Two copies a full span apart, so a bank never pops in at the edge
        // of the frame -- one is always sliding in as the other slides out.
        DrawHazeBank4(drift,        cy, rx, ry, cr, cg, cb, a);
        DrawHazeBank4(drift - span, cy, rx, ry, cr, cg, cb, a);
    }

    // Warm city glow sitting on the horizon: cold night above, distant
    // sodium-lit town below, which gives the sky somewhere to end.
    glBegin(GL_QUADS);
        glColor4ub(MixB4(90, 150), MixB4(60, 190), MixB4(55, 240), (unsigned char)(MixB4(0, 80) * (1.0f - dayBlend * 0.25f)));
        glVertex2f(-60, 10); glVertex2f(60, 10);
        glColor4ub(MixB4(150, 160), MixB4(95, 215), MixB4(60, 220), (unsigned char)(MixB4(120, 90) * (1.0f - 0.65f * dayBlend)));
        glVertex2f(60, -6);  glVertex2f(-60, -6);
    glEnd();

    if (dayBlend > 0.02f) {
        glColor4ub(255, 220, 160, (unsigned char)(40.0f * dayBlend));
        glBegin(GL_QUADS);
            glVertex2f(-60, 4.0f); glVertex2f(60, 4.0f);
            glVertex2f(60, -6.0f); glVertex2f(-60, -6.0f);
        glEnd();
    }
}

// ---- Moon: the scene's cool light source ---------------------------------
constexpr float moonX4 = 34.0f, moonY4 = 27.0f, moonR4 = 3.1f;

void DrawMoon4() {
    float nightFade = NightT4();
    if (dayT4 > 0.05f) {
        float sunA = dayT4 * 255.0f;
        // Wide, very soft daylight bloom: the sun should light the air around
        // it, not sit on the sky as a flat yellow disc.
        // Radius grows quadratically while alpha falls off smoothly; equal
            // steps in both produced a visible set of concentric rings.
        DrawRadialGlow4(moonX4, moonY4, moonR4 * 9.0f, moonR4 * 9.0f,
                        255, 226, 160, (unsigned char)(58.0f * dayT4));
        DrawRadialGlow4(moonX4, moonY4, moonR4 * 3.4f, moonR4 * 3.4f,
                        255, 232, 176, (unsigned char)(96.0f * dayT4));
        DrawRadialGlow4(moonX4, moonY4, moonR4 * 1.7f, moonR4 * 1.7f,
                        255, 238, 194, (unsigned char)(130.0f * dayT4));
        glColor4ub(255, 221, 150, (unsigned char)sunA);
        glBegin(GL_TRIANGLE_FAN);
            for (int j = 0; j <= 24; j++) {
                float ang = (float)j / 24.0f * 6.2831853f;
                glVertex2f(moonX4 + moonR4 * cosf(ang), moonY4 + moonR4 * sinf(ang));
            }
        glEnd();
        return;
    }

    // Atmospheric illumination: a very wide disc of faintly lit air, then a
    // tighter halo on top of it. Both are smooth radial falloffs -- the old
    // stack of twelve 20-sided circles put visible rings round the moon.
    DrawRadialGlow4(moonX4, moonY4, moonR4 * 8.5f, moonR4 * 8.5f,
                    150, 178, 232, (unsigned char)(40 * nightFade));
    DrawRadialGlow4(moonX4, moonY4, moonR4 * 3.6f, moonR4 * 3.6f,
                    196, 216, 248, (unsigned char)(70 * nightFade));
    DrawRadialGlow4(moonX4, moonY4, moonR4 * 1.85f, moonR4 * 1.85f,
                    226, 238, 254, (unsigned char)(105 * nightFade));

    FilledCircle4(moonX4, moonY4, moonR4, 238, 243, 252, (unsigned char)(255 * nightFade));
    // The limb picks up a touch less light than the centre, which is what
    // keeps the disc from reading as a paper cut-out.
    FilledCircle4(moonX4 + moonR4 * 0.16f, moonY4 + moonR4 * 0.14f, moonR4 * 0.82f,
                  248, 251, 255, (unsigned char)(190 * nightFade));
    // Craters
    FilledCircle4(moonX4 - 0.9f, moonY4 + 0.7f, 0.62f, 214, 222, 236, (unsigned char)(235 * nightFade));
    FilledCircle4(moonX4 + 1.0f, moonY4 - 0.5f, 0.45f, 218, 226, 239, (unsigned char)(235 * nightFade));
    FilledCircle4(moonX4 + 0.2f, moonY4 + 1.5f, 0.30f, 220, 228, 241, (unsigned char)(235 * nightFade));
    FilledCircle4(moonX4 - 1.3f, moonY4 - 1.2f, 0.26f, 220, 228, 241, (unsigned char)(235 * nightFade));
}

void DrawStars4() {
    float night = NightT4();
    if (night <= 0.02f) return;      // daylight washes the field out entirely

    // glPointSize cannot change inside a glBegin block, so the field is drawn
    // in three passes bucketed by size rather than one pass of equal dots.
    const float sizes[3] = { 1.1f, 1.8f, 2.7f };
    for (int pass = 0; pass < 3; pass++) {
        glPointSize(sizes[pass]);
        glBegin(GL_POINTS);
        for (int i = 0; i < NUM_STARS4; i++) {
            const Star4& st = stars4[i];
            if (st.bucket != pass) continue;
            float tw = 0.55f + 0.45f * sinf(twinklePhase4 * st.rate + st.twinkle);
            // Stars thin out into the horizon haze rather than stopping dead
            // at the top of the rooflines.
            float hz = (st.y - 4.0f) / 16.0f;
            if (hz < 0.0f) hz = 0.0f;
            if (hz > 1.0f) hz = 1.0f;
            float a = st.mag * tw * night * (0.22f + 0.78f * hz);
            glColor4ub(st.r, st.g, st.b, (unsigned char)(255.0f * a));
            glVertex2f(st.x, st.y);
        }
        glEnd();
    }
    glPointSize(1.0f);

    // The bright few, each with a halo and a faint cross flare.
    for (int i = 0; i < NUM_BRIGHT_STARS4; i++) {
        const BrightStar4& b = brightStars4[i];
        float tw = 0.58f + 0.42f * sinf(twinklePhase4 * 1.6f + b.phase);
        float a  = night * tw;
        FilledCircle4(b.x, b.y, b.r * 3.0f, 150, 180, 235, (unsigned char)(20 * a));
        FilledCircle4(b.x, b.y, b.r * 1.4f, 206, 224, 250, (unsigned char)(52 * a));
        FilledCircle4(b.x, b.y, b.r * 0.55f, 255, 255, 255, (unsigned char)(240 * a));
        glColor4ub(240, 246, 255, (unsigned char)(80 * a));
        glLineWidth(1.0f);
        glBegin(GL_LINES);
            glVertex2f(b.x - b.r * 2.6f, b.y); glVertex2f(b.x + b.r * 2.6f, b.y);
            glVertex2f(b.x, b.y - b.r * 2.6f); glVertex2f(b.x, b.y + b.r * 2.6f);
        glEnd();
    }
}

// ============================================================================
//  FLYING BIRDS -- distant silhouettes crossing the sky
// ----------------------------------------------------------------------------
//  Drawn after the sky and before the skyline, so the rooflines cut them off
//  and they stay where they belong: in the air behind the town, never over
//  the market. Each bird is a shallow "M" whose wing tips rise and fall; at
//  this size that is all a flapping bird needs to be.
// ============================================================================
void RespawnBird4(Bird4& b, float atX) {
    b.x         = atX;
    b.y         = 12.0f + (rand() % 210) / 10.0f;        // 12 .. 33
    b.speed     = 0.10f + (rand() % 22) / 100.0f;
    b.scale     = 0.55f + (rand() % 70) / 100.0f;
    b.flapRate  = 0.16f + (rand() % 16) / 100.0f;
    b.flapPhase = (rand() % 628) / 100.0f;
    b.bobPhase  = (rand() % 628) / 100.0f;
    b.bobAmp    = 0.25f + (rand() % 60) / 100.0f;
}

void DrawBirds4() {
    // By day they are legible dark-grey silhouettes against a bright sky. By
    // night they drop to a faint shade barely separated from the air, which is
    // all a bird would be at that distance -- solid black shapes on a night
    // sky read as confetti, not wildlife.
    // At night they are a shade LIGHTER than the sky, not darker: at this
    // distance what you actually see is moonlight catching the underside of a
    // wing. A near-black bird on a near-black sky is simply not there.
    unsigned char br = MixB4(112, 48), bg = MixB4(124, 52), bb = MixB4(152, 62);
    unsigned char alpha = MixB4(120, 215);

    glLineWidth(1.4f);
    for (int i = 0; i < NUM_BIRDS4; i++) {
        const Bird4& bd = birds4[i];
        float sp   = bd.scale;
        float y    = bd.y + sinf(bd.bobPhase) * bd.bobAmp;
        float flap = sinf(bd.flapPhase);                  // -1 .. 1
        float up   = 0.30f + 0.70f * (0.5f + 0.5f * flap);// wing tip height
        // Distant birds are fainter as well as smaller: one more depth cue
        // for almost no cost.
        unsigned char a = (unsigned char)(alpha * (0.45f + 0.55f * sp));

        glColor4ub(br, bg, bb, a);
        glBegin(GL_LINE_STRIP);
            glVertex2f(bd.x - 1.20f * sp, y + up * 0.80f * sp);
            glVertex2f(bd.x - 0.50f * sp, y + (up * 0.16f - 0.06f) * sp);
            glVertex2f(bd.x,              y + 0.10f * sp);
            glVertex2f(bd.x + 0.50f * sp, y + (up * 0.16f - 0.06f) * sp);
            glVertex2f(bd.x + 1.20f * sp, y + up * 0.80f * sp);
        glEnd();
        // A short body stub ahead of the wings, so the shape has a direction
        // of travel instead of being a symmetrical tick mark.
        glBegin(GL_LINES);
            glVertex2f(bd.x, y + 0.10f * sp);
            glVertex2f(bd.x + 0.34f * sp * bd.dir, y + 0.14f * sp);
        glEnd();
    }
    glLineWidth(1.0f);
}

void UpdateBirds4(int) {
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateBirds4, 0); return; }
    if (isAnimating4) {
        for (int i = 0; i < NUM_BIRDS4; i++) {
            Bird4& b = birds4[i];
            b.x         += b.speed * (float)b.dir;
            b.flapPhase += b.flapRate;
            b.bobPhase  += 0.035f;
            if (b.dir > 0 && b.x >  70.0f) RespawnBird4(b, -70.0f);
            if (b.dir < 0 && b.x < -70.0f) RespawnBird4(b,  70.0f);
        }
    }
    glutTimerFunc(45, UpdateBirds4, 0);
}

// ============================================================================
//  FLYING SLEIGH -- signature feature: Santa's sleigh and reindeer, always
//  silhouetted against the night sky with a sparkling trail behind it.
// ============================================================================
void DrawSleigh4() {
    glPushMatrix();
    glTranslatef(sleighX4, sleighY4, 0.0f);
    glScalef(sleighScale4, sleighScale4, 1.0f);
    glColor3ub(15, 15, 22);

    // Three reindeer, in a line ahead of the sleigh
    for (int i = 0; i < 3; i++) {
        float dx = -i * 2.2f;
        glBegin(GL_QUADS);
            glVertex2f(dx-0.9f, 0.3f); glVertex2f(dx+0.9f, 0.3f);
            glVertex2f(dx+0.9f, 0.9f); glVertex2f(dx-0.9f, 0.9f);
        glEnd();
        glLineWidth(1.5f);
        glBegin(GL_LINES);
            glVertex2f(dx-0.6f, 0.3f); glVertex2f(dx-0.7f, -0.3f);
            glVertex2f(dx+0.5f, 0.3f); glVertex2f(dx+0.6f, -0.3f);
        glEnd();
        glBegin(GL_TRIANGLES);
            glVertex2f(dx+0.9f, 0.9f); glVertex2f(dx+1.4f, 1.1f); glVertex2f(dx+0.9f, 0.6f);
        glEnd();
        glBegin(GL_LINES);
            glVertex2f(dx+1.2f, 1.05f); glVertex2f(dx+1.5f, 1.5f);
            glVertex2f(dx+1.2f, 1.05f); glVertex2f(dx+1.0f, 1.5f);
        glEnd();
    }
    // Traces linking each reindeer back to the sleigh -- previously the team
    // and the sleigh were simply adjacent with nothing connecting them.
    glColor3ub(120, 85, 50);
    glLineWidth(1.0f);
    glBegin(GL_LINE_STRIP);
        glVertex2f(1.2f, 0.6f);
        glVertex2f(-1.0f, 0.6f);
        glVertex2f(-3.2f, 0.6f);
        glVertex2f(-5.4f, 0.6f);
        glVertex2f(-6.2f, 0.7f);
    glEnd();

    // Rudolph leads: red nose plus a glow that lights the air ahead.
    FilledCircle4(1.55f, 1.12f, 0.55f, 255, 70, 60, 60);
    FilledCircle4(1.55f, 1.12f, 0.28f, 255, 90, 70, 130);
    FilledCircle4(1.55f, 1.12f, 0.15f, 255, 160, 140, 255);

    // Sleigh body
    glColor3ub(15, 15, 22);
    glBegin(GL_POLYGON);
        glVertex2f(-8.0f, 0.3f); glVertex2f(-6.0f, 0.3f);
        glVertex2f(-6.0f, 1.0f); glVertex2f(-7.0f, 1.0f);
        glVertex2f(-8.5f, 0.6f);
    glEnd();
    // Sparkling trail
    for (int i = 0; i < 6; i++) {
        float t = fmodf(sleighTrailPhase4 + i*0.3f, 1.0f);
        float tx = -8.0f - t*6.0f;
        float ty = 0.6f + sinf(t*10.0f)*0.3f;
        FilledCircle4(tx, ty, 0.15f*(1.0f-t), 255, 240, 200, (unsigned char)(220*(1.0f-t)));
    }
    glPopMatrix();
}

void UpdateSleigh4(int) {
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateSleigh4, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating4) {
        sleighX4 += 0.25f;
        sleighTrailPhase4 += 0.05f;
        if (sleighX4 > 100.0f) {
            sleighX4 = -100.0f - (rand() % 30);
            // Flies a little higher than before, because at the larger size
            // the reindeer team would otherwise graze the tallest shop roof
            // (the CAFE eave reaches y = +16).
            sleighY4 = 25.0f + (rand() % 9);
        }
    }
    glutTimerFunc(30, UpdateSleigh4, 0);

}

void UpdateMetroRail4(int) {
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(60, UpdateMetroRail4, 0); return; }
    // Advance global time (approx seconds per 60ms tick)
    if (isAnimating4) metroTime4 += 0.06f;

    if (!metroActive4) {
        if (metroTime4 >= metroNextStart4) {
            metroActive4 = true;
            metroCarX4 = -80.0f; // start well off-screen left
        }
    } else {
        metroCarX4 += metroSpeed4; // move rightwards
        if (metroCarX4 > 80.0f) {
            metroActive4 = false;
            // schedule next pass in ~20 seconds
            metroNextStart4 = metroTime4 + 20.0f;
        }
    }
    glutPostRedisplay();
    glutTimerFunc(60, UpdateMetroRail4, 0);
}

// ---- Shop facade ---------------------------------------------------------
// The row is the backdrop the whole market stands against, so each building
// is drawn the way one actually stacks, from the street up: glazed shopfront
// and door, awning, name board, two storeys of windows, then the gable and
// the snow on it. Every dimension is a fraction of the entry's w and h, so
// resizing a shop in shops4[] rescales the whole facade instead of stretching
// one band of it out of proportion.
constexpr float SHOP_BASE_Y4    = -6.0f;   // the row stands on the plaza horizon
constexpr float SHOP_EAVE_OVER4 =  0.6f;   // roof overhang past each end of the wall

inline float ShopEaveY4(const Shop4& s)    { return SHOP_BASE_Y4 + s.h; }
inline float ShopRoofRise4(const Shop4& s) { return 2.1f + s.w * 0.10f; }

// Height of the roof surface above world x. The chimney and its smoke use it
// so they sit on the slope instead of floating at the eave line.
inline float ShopRoofY4(const Shop4& s, float x) {
    float half = s.w * 0.5f + SHOP_EAVE_OVER4;
    float t    = fabsf(x - s.x) / half;
    if (t > 1.0f) t = 1.0f;
    return ShopEaveY4(s) + ShopRoofRise4(s) * (1.0f - t);
}

// One storey of windows. `shift` rotates which of the five lit[] flags each
// window takes, so the two rows light up in different patterns from a single
// five-bit state instead of every floor matching.
void DrawShopWindowRow4(const Shop4& s, float y, float wh, int shift) {
    const int   n    = 5;
    const float slot = s.w / (float)(n + 1);
    const float ww   = slot * 0.52f;
    for (int i = 0; i < n; i++) {
        float wx = s.x - s.w*0.5f + slot*(i+1) - ww*0.5f;
        bool  on = s.lit[(i + shift) % 5];
        if (on) {
            // A lit room throws light onto the wall around its opening --
            // without this the windows read as stickers on a flat board.
            glColor4ub(255, 196, 110, (unsigned char)(38.0f * (0.35f + 0.65f * NightT4())));
            glBegin(GL_QUADS);
                glVertex2f(wx-0.7f,    y-0.7f);     glVertex2f(wx+ww+0.7f, y-0.7f);
                glVertex2f(wx+ww+0.7f, y+wh+0.7f);  glVertex2f(wx-0.7f,    y+wh+0.7f);
            glEnd();
            glColor3ub(255, 210, 134);
        } else {
            // Unlit glass: near-black at night, a cool reflection of the sky
            // by day. Leaving it black through a daylight frame is what made
            // the row look like it had been cut out of the night sky.
            glColor3ub(MixB4(17, 74), MixB4(15, 88), MixB4(25, 112));
        }
        glBegin(GL_QUADS);
            glVertex2f(wx,    y);      glVertex2f(wx+ww, y);
            glVertex2f(wx+ww, y+wh);   glVertex2f(wx,    y+wh);
        glEnd();
        // Frame and glazing bars. At this size they are what makes the
        // opening read as a sash window rather than a yellow rectangle.
        glColor3ub(MixB4(82, 168), MixB4(70, 156), MixB4(72, 156));
        glLineWidth(1.3f);
        glBegin(GL_LINE_LOOP);
            glVertex2f(wx,    y);      glVertex2f(wx+ww, y);
            glVertex2f(wx+ww, y+wh);   glVertex2f(wx,    y+wh);
        glEnd();
        glBegin(GL_LINES);
            glVertex2f(wx+ww*0.5f, y);         glVertex2f(wx+ww*0.5f, y+wh);
            glVertex2f(wx,         y+wh*0.55f); glVertex2f(wx+ww,     y+wh*0.55f);
        glEnd();
        // Sill, so each storey has a horizontal line to sit on
        glColor3ub(MixB4(96, 196), MixB4(88, 188), MixB4(96, 190));
        glBegin(GL_QUADS);
            glVertex2f(wx-0.25f, y-0.30f); glVertex2f(wx+ww+0.25f, y-0.30f);
            glVertex2f(wx+ww+0.25f, y);    glVertex2f(wx-0.25f,    y);
        glEnd();
        glLineWidth(1.0f);
    }
}

void DrawShop4(const Shop4& s) {
    const float baseY = SHOP_BASE_Y4;
    const float halfW = s.w * 0.5f;
    const float eaveY = ShopEaveY4(s);
    const float rise  = ShopRoofRise4(s);
    const float over  = SHOP_EAVE_OVER4;

    // ---- Receding side wall ------------------------------------------------
    // The row used to be seven flat boards standing edge-on to the camera. A
    // building only reads as a solid once you can see round one corner of it,
    // so each shop now shows the flank that faces the middle of the frame,
    // thrown toward the vanishing point at x = 0. Shops far out to the sides
    // show a wide flank, shops near the centre almost none -- which is what
    // single-point perspective actually does, and it costs one quad.
    //
    // The gaps between roof spans are 2.2 units at the tightest, and sideW is
    // capped at 2.0, so a flank can never reach under its neighbour's wall.
    {
        float dir  = (s.x < 0.0f) ? 1.0f : -1.0f;
        float fx   = s.x + dir * halfW;                  // the corner we see round
        float sideW = fabsf(fx) * 0.030f;
        if (sideW > 2.0f)  sideW = 2.0f;
        if (sideW > 0.22f) {
            float k       = sideW / (fabsf(fx) + 0.001f);
            float backEave = eaveY + (baseY - eaveY) * k;   // converges on the horizon
            // The light is up and to the right in both day and night, so a
            // right-facing flank is lit and a left-facing one is in shade.
            bool lit = (dir > 0.0f);
            glBegin(GL_QUADS);
                if (lit) glColor3ub(MixB4(24, 84),  MixB4(21, 78),  MixB4(31, 90));
                else     glColor3ub(MixB4(13, 52),  MixB4(11, 48),  MixB4(18, 58));
                glVertex2f(fx,           baseY);
                glVertex2f(fx + dir*sideW, baseY);
                if (lit) glColor3ub(MixB4(41, 128), MixB4(36, 120), MixB4(52, 134));
                else     glColor3ub(MixB4(22, 78),  MixB4(19, 72),  MixB4(29, 84));
                glVertex2f(fx + dir*sideW, backEave);
                glVertex2f(fx,             eaveY);
            glEnd();
            // Snow on the flank's own eave, following the same convergence.
            glColor3ub(MixB4(196, 232), MixB4(206, 238), MixB4(224, 248));
            float cap = 0.30f + 0.55f * SnowDepth4();
            glBegin(GL_QUADS);
                glVertex2f(fx,             eaveY);
                glVertex2f(fx + dir*sideW, backEave);
                glVertex2f(fx + dir*sideW, backEave + cap * 0.7f);
                glVertex2f(fx,             eaveY + cap);
            glEnd();
        }
    }

    // ---- Wall -------------------------------------------------------------
    // Graded dark-to-light going up: a single flat fill on a wall this tall
    // reads as a hole cut in the sky rather than as masonry. The pair of
    // colours is a night/day blend now, so by daylight the row is warm plaster
    // under a blue sky instead of the same midnight browns lit from nowhere.
    glBegin(GL_QUADS);
        glColor3ub(MixB4(29, 118), MixB4(25, 110), MixB4(37, 116));
        glVertex2f(s.x-halfW, baseY);  glVertex2f(s.x+halfW, baseY);
        glColor3ub(MixB4(48, 168), MixB4(42, 160), MixB4(60, 164));
        glVertex2f(s.x+halfW, eaveY);  glVertex2f(s.x-halfW, eaveY);
    glEnd();

    // ---- Directional shading on the front face ------------------------------
    // Three washes, all keyed to the same light: a lit band down the right of
    // the wall, a shadow band down the left, and the shadow the roof overhang
    // drops onto the top of the wall. Together they are what stops the facade
    // from reading as a printed panel.
    glBegin(GL_QUADS);                                  // lit edge, from the right
        glColor4ub(255, MixB4(226, 244), MixB4(190, 226), (unsigned char)(MixB4(30, 54)));
        glVertex2f(s.x+halfW, baseY); glVertex2f(s.x+halfW, eaveY);
        glColor4ub(255, MixB4(226, 244), MixB4(190, 226), 0);
        glVertex2f(s.x+halfW-s.w*0.34f, eaveY); glVertex2f(s.x+halfW-s.w*0.34f, baseY);
    glEnd();
    glBegin(GL_QUADS);                                  // shaded edge, on the left
        glColor4ub(MixB4(4, 40), MixB4(6, 48), MixB4(14, 70), (unsigned char)(MixB4(120, 86)));
        glVertex2f(s.x-halfW, baseY); glVertex2f(s.x-halfW, eaveY);
        glColor4ub(MixB4(4, 40), MixB4(6, 48), MixB4(14, 70), 0);
        glVertex2f(s.x-halfW+s.w*0.40f, eaveY); glVertex2f(s.x-halfW+s.w*0.40f, baseY);
    glEnd();
    glBegin(GL_QUADS);                                  // roof overhang shadow
        glColor4ub(MixB4(4, 38), MixB4(6, 44), MixB4(14, 62), (unsigned char)(MixB4(135, 105)));
        glVertex2f(s.x-halfW, eaveY); glVertex2f(s.x+halfW, eaveY);
        glColor4ub(MixB4(4, 38), MixB4(6, 44), MixB4(14, 62), 0);
        glVertex2f(s.x+halfW, eaveY-2.6f); glVertex2f(s.x-halfW, eaveY-2.6f);
    glEnd();
    glBegin(GL_QUADS);                                  // ambient occlusion at the foot
        glColor4ub(MixB4(4, 44), MixB4(6, 50), MixB4(14, 68), (unsigned char)(MixB4(105, 72)));
        glVertex2f(s.x-halfW, baseY); glVertex2f(s.x+halfW, baseY);
        glColor4ub(MixB4(4, 44), MixB4(6, 50), MixB4(14, 68), 0);
        glVertex2f(s.x+halfW, baseY+1.8f); glVertex2f(s.x-halfW, baseY+1.8f);
    glEnd();

    // Corner pilasters. Neighbouring shops are only ~2.6 apart, so without a
    // lit edge on each end the row merges back into one dark terrace.
    glColor3ub(MixB4(64, 186), MixB4(56, 178), MixB4(74, 182));
    glBegin(GL_QUADS);
        glVertex2f(s.x-halfW,       baseY); glVertex2f(s.x-halfW+0.45f, baseY);
        glVertex2f(s.x-halfW+0.45f, eaveY); glVertex2f(s.x-halfW,       eaveY);
        glVertex2f(s.x+halfW-0.45f, baseY); glVertex2f(s.x+halfW,       baseY);
        glVertex2f(s.x+halfW,       eaveY); glVertex2f(s.x+halfW-0.45f, eaveY);
    glEnd();

    // ---- Upper storeys ----------------------------------------------------
    float winH = s.h * 0.125f;
    DrawShopWindowRow4(s, baseY + s.h * 0.50f, winH, 0);
    DrawShopWindowRow4(s, baseY + s.h * 0.72f, winH, 2);

    // Storey band dividing the shopfront from the rooms above it
    float bandY = baseY + s.h * 0.41f;
    glColor3ub(MixB4(66, 176), MixB4(58, 168), MixB4(76, 172));
    glBegin(GL_QUADS);
        glVertex2f(s.x-halfW, bandY);       glVertex2f(s.x+halfW, bandY);
        glVertex2f(s.x+halfW, bandY+0.4f);  glVertex2f(s.x-halfW, bandY+0.4f);
    glEnd();

    // ---- Street level -----------------------------------------------------
    // The glazed front and its door are the part that says "shop": a tall
    // wall of windows on its own would just be a tenement.
    float frontTop = baseY + s.h * 0.26f;
    float frontIn  = halfW - 0.9f;
    float doorW    = s.w * 0.16f;
    float glassL   = s.x - frontIn;
    float glassR   = s.x + frontIn - doorW - 0.7f;

    glColor4ub(255, 198, 118, (unsigned char)(66.0f * (0.30f + 0.70f * NightT4())));  // warm spill onto the paving
    glBegin(GL_QUADS);
        glVertex2f(glassL-1.2f, baseY-1.4f); glVertex2f(s.x+frontIn+1.2f, baseY-1.4f);
        glVertex2f(s.x+frontIn+1.2f, frontTop+0.6f); glVertex2f(glassL-1.2f, frontTop+0.6f);
    glEnd();

    glColor3ub(255, 214, 148);                      // the glazing
    glBegin(GL_QUADS);
        glVertex2f(glassL, baseY+0.35f); glVertex2f(glassR, baseY+0.35f);
        glVertex2f(glassR, frontTop);    glVertex2f(glassL, frontTop);
    glEnd();

    // Shoppers moving about inside, seen as silhouettes through the glass
    for (int i = 0; i < 2; i++) {
        float t  = glassL + (glassR - glassL) * (0.3f + 0.4f * i)
                 + sinf(twinklePhase4 * 0.5f + i * 2.1f) * s.w * 0.05f;
        float fh = (frontTop - baseY) * 0.55f;
        glColor4ub(60, 40, 34, 190);
        glBegin(GL_QUADS);
            glVertex2f(t-0.45f, baseY+0.35f); glVertex2f(t+0.45f, baseY+0.35f);
            glVertex2f(t+0.45f, baseY+0.35f+fh); glVertex2f(t-0.45f, baseY+0.35f+fh);
        glEnd();
        FilledCircle4(t, baseY+0.35f+fh+0.35f, 0.38f, 60, 40, 34, 190);
    }

    // Mullions, drawn over the silhouettes so the glass stays in front
    glColor3ub(58, 44, 38);
    glLineWidth(1.5f);
    glBegin(GL_LINES);
        for (int i = 1; i <= 3; i++) {
            float mx = glassL + (glassR - glassL) * i / 4.0f;
            glVertex2f(mx, baseY+0.35f); glVertex2f(mx, frontTop);
        }
        glVertex2f(glassL, frontTop-(frontTop-baseY)*0.22f);
        glVertex2f(glassR, frontTop-(frontTop-baseY)*0.22f);
    glEnd();
    glBegin(GL_LINE_LOOP);
        glVertex2f(glassL, baseY+0.35f); glVertex2f(glassR, baseY+0.35f);
        glVertex2f(glassR, frontTop);    glVertex2f(glassL, frontTop);
    glEnd();

    // Door, with a lit fanlight and a wreath on it
    float doorL = glassR + 0.7f, doorR = doorL + doorW;
    glColor3ub(92, 58, 44);
    glBegin(GL_QUADS);
        glVertex2f(doorL, baseY+0.35f); glVertex2f(doorR, baseY+0.35f);
        glVertex2f(doorR, frontTop);    glVertex2f(doorL, frontTop);
    glEnd();
    glColor3ub(255, 226, 170);
    glBegin(GL_QUADS);
        glVertex2f(doorL+0.18f, frontTop-0.95f); glVertex2f(doorR-0.18f, frontTop-0.95f);
        glVertex2f(doorR-0.18f, frontTop-0.18f); glVertex2f(doorL+0.18f, frontTop-0.18f);
    glEnd();
    float wreathY = baseY + (frontTop-baseY) * 0.55f;
    FilledCircle4((doorL+doorR)*0.5f, wreathY, doorW*0.30f, 52, 112, 68, 255);
    FilledCircle4((doorL+doorR)*0.5f, wreathY, doorW*0.16f,  92, 58, 44, 255);

    // ---- Awning -----------------------------------------------------------
    // Striped, in a colour taken from the shop's index so the row does not
    // repeat, and scalloped along its lower edge.
    float awnY    = frontTop;
    float awnH    = 1.20f;
    float awnHalf = halfW - 0.55f;
    int   stripes = 9;
    for (int i = 0; i < stripes; i++) {
        float x0 = s.x - awnHalf + (2.0f*awnHalf) * i     / stripes;
        float x1 = s.x - awnHalf + (2.0f*awnHalf) * (i+1) / stripes;
        if (i % 2 == 0) glColor3ub(186, 62, 62);
        else            glColor3ub(238, 238, 240);
        glBegin(GL_QUADS);
            glVertex2f(x0, awnY); glVertex2f(x1, awnY);
            glVertex2f(x1, awnY+awnH); glVertex2f(x0, awnY+awnH);
        glEnd();
        glBegin(GL_TRIANGLES);              // scallop hanging below the awning
            glVertex2f(x0, awnY); glVertex2f(x1, awnY);
            glVertex2f((x0+x1)*0.5f, awnY-0.45f);
        glEnd();
    }
    glColor3ub(46, 40, 52);                 // the rail the awning hangs from
    glBegin(GL_QUADS);
        glVertex2f(s.x-awnHalf, awnY+awnH);       glVertex2f(s.x+awnHalf, awnY+awnH);
        glVertex2f(s.x+awnHalf, awnY+awnH+0.28f); glVertex2f(s.x-awnHalf, awnY+awnH+0.28f);
    glEnd();

    // ---- Name board -------------------------------------------------------
    // The board is sized from the text it has to hold, not from the shop, so
    // a long name can never run past its own frame or off the side of the
    // building the way a fixed fraction of s.w did.
    if (s.sign != nullptr) {
        const char* name = s.sign;
        float ty    = awnY + awnH + 0.85f;
        float signH = 2.2f;
        float txtW  = TextPixelWidth(GLUT_BITMAP_HELVETICA_12, name)
                    * (WORLD_RIGHT - WORLD_LEFT) / (float)viewportPixelWidth;
        float half  = txtW * 0.5f + 1.4f;

        glColor4ub(255, 198, 112, 52);      // the board's own light on the wall
        glBegin(GL_QUADS);
            glVertex2f(s.x-half-1.4f, ty-1.1f);        glVertex2f(s.x+half+1.4f, ty-1.1f);
            glVertex2f(s.x+half+1.4f, ty+signH+1.1f);  glVertex2f(s.x-half-1.4f, ty+signH+1.1f);
        glEnd();
        glColor3ub(48, 36, 28);
        glBegin(GL_QUADS);
            glVertex2f(s.x-half, ty);         glVertex2f(s.x+half, ty);
            glVertex2f(s.x+half, ty+signH);   glVertex2f(s.x-half, ty+signH);
        glEnd();
        glColor3ub(206, 164, 100);
        glLineWidth(1.6f);
        glBegin(GL_LINE_LOOP);
            glVertex2f(s.x-half, ty);         glVertex2f(s.x+half, ty);
            glVertex2f(s.x+half, ty+signH);   glVertex2f(s.x-half, ty+signH);
        glEnd();
        // Brackets tying the board back to the wall
        glBegin(GL_LINES);
            glVertex2f(s.x-half, ty+signH); glVertex2f(s.x-half-0.9f, ty+signH+0.9f);
            glVertex2f(s.x+half, ty+signH); glVertex2f(s.x+half+0.9f, ty+signH+0.9f);
        glEnd();
        glLineWidth(1.0f);
        // The name itself, one point larger than the stall labels in front of
        // it so the backdrop row stays legible behind the crowd.
        glColor3ub(255, 232, 178);
        DrawTextCentered(s.x, ty + signH*0.5f - 0.55f, GLUT_BITMAP_HELVETICA_12, name);
    }

    // ---- Roof -------------------------------------------------------------
    // Dark fascia under the eaves first, so the wall and the snow cap that
    // DrawSnowAccumulation4 lays along the eave line never meet directly.
    glColor3ub(MixB4(38, 82), MixB4(33, 76), MixB4(48, 88));
    glBegin(GL_QUADS);
        glVertex2f(s.x-halfW-over, eaveY-0.6f); glVertex2f(s.x+halfW+over, eaveY-0.6f);
        glVertex2f(s.x+halfW+over, eaveY);      glVertex2f(s.x-halfW-over, eaveY);
    glEnd();
    // Gable. The rise scales with the width, so a wide shop gets a roof in
    // proportion instead of the same small cap a narrow one wears. The two
    // slopes are drawn separately because they face opposite ways: with the
    // light up and to the right, one of them catches it and one does not, and
    // a single flat triangle across both is the fastest way to lose the roof's
    // shape entirely.
    glColor3ub(MixB4(40, 104), MixB4(35, 96), MixB4(50, 108));      // shaded slope
    glBegin(GL_TRIANGLES);
        glVertex2f(s.x-halfW-over, eaveY);
        glVertex2f(s.x,            eaveY);
        glVertex2f(s.x,            eaveY+rise);
    glEnd();
    glColor3ub(MixB4(66, 150), MixB4(58, 142), MixB4(80, 150));      // lit slope
    glBegin(GL_TRIANGLES);
        glVertex2f(s.x,            eaveY);
        glVertex2f(s.x+halfW+over, eaveY);
        glVertex2f(s.x,            eaveY+rise);
    glEnd();
    // Snow lying on both slopes, again split: settled snow in shade goes blue,
    // snow facing the light stays near white.
    glColor3ub(MixB4(200, 214), MixB4(212, 226), MixB4(234, 242));
    glBegin(GL_TRIANGLES);
        glVertex2f(s.x-halfW-over+0.55f, eaveY+0.75f);
        glVertex2f(s.x,                  eaveY+0.75f);
        glVertex2f(s.x,                  eaveY+rise+0.40f);
    glEnd();
    glColor3ub(238, 243, 252);
    glBegin(GL_TRIANGLES);
        glVertex2f(s.x,                  eaveY+0.75f);
        glVertex2f(s.x+halfW+over-0.55f, eaveY+0.75f);
        glVertex2f(s.x,                  eaveY+rise+0.40f);
    glEnd();
    // Attic light in the gable, lit whenever the middle window is
    if (s.lit[2]) FilledCircle4(s.x, eaveY + rise*0.42f, 0.45f, 255, 206, 128, 255);
    else          FilledCircle4(s.x, eaveY + rise*0.42f, 0.45f,  26,  23,  34, 255);
}

void DrawShops4() { for (int i = 0; i < NUM_SHOPS4; i++) DrawShop4(shops4[i]); }

// The chimney stands on GIFTS. Now that the roof is a gable rather than a
// flat top, its foot has to follow the slope at that x or it hangs in the air
// off the side of the ridge.
inline float ChimneyX4() { return shops4[1].x + shops4[1].w * 0.26f; }

void DrawChimney4() {
    float x    = ChimneyX4();
    float foot = ShopRoofY4(shops4[1], x) - 0.3f;       // buried a little in the slope
    float top  = foot + 2.8f;
    glColor3ub(58, 52, 62);
    glBegin(GL_QUADS);
        glVertex2f(x-0.7f, foot); glVertex2f(x+0.7f, foot);
        glVertex2f(x+0.7f, top);  glVertex2f(x-0.7f, top);
    glEnd();
    glColor3ub(74, 66, 78);                             // capping course
    glBegin(GL_QUADS);
        glVertex2f(x-0.95f, top);       glVertex2f(x+0.95f, top);
        glVertex2f(x+0.95f, top+0.35f); glVertex2f(x-0.95f, top+0.35f);
    glEnd();
    glColor3ub(238, 243, 252);                          // snow on the cap
    glBegin(GL_QUADS);
        glVertex2f(x-0.95f, top+0.35f); glVertex2f(x+0.95f, top+0.35f);
        glVertex2f(x+0.75f, top+0.75f); glVertex2f(x-0.75f, top+0.75f);
    glEnd();
}

void DrawLightSpan4(const LightSpan4& span, int spanIdx) {
    glColor3ub(60, 50, 40);
    glLineWidth(1.0f);
    glBegin(GL_LINE_STRIP);
        for (int i = 0; i <= BULBS_PER_SPAN4+1; i++) {
            float t = (float)i / (BULBS_PER_SPAN4+1);
            float x = span.x1 + (span.x2-span.x1)*t;
            float y = span.y1 + (span.y2-span.y1)*t - sinf(t*PI4)*1.5f;
            glVertex2f(x, y);
        }
    glEnd();
    for (int i = 0; i < BULBS_PER_SPAN4; i++) {
        float t = (float)(i+1) / (BULBS_PER_SPAN4+1);
        float x = span.x1 + (span.x2-span.x1)*t;
        float y = span.y1 + (span.y2-span.y1)*t - sinf(t*PI4)*1.5f;
        unsigned char r, g, b;
        if (multicolorLights4) {
            int c = i % 4;
            unsigned char cols[4][3] = { {230,60,60}, {60,200,90}, {60,140,230}, {240,210,60} };
            r = cols[c][0]; g = cols[c][1]; b = cols[c][2];
        } else { r = 255; g = 210; b = 140; }
        float on = bulbLit4[spanIdx][i] ? 1.0f : 0.35f;
        FilledCircle4(x, y, 0.28f, (unsigned char)(r*on), (unsigned char)(g*on), (unsigned char)(b*on), 255);
    }
}

void DrawLightSpans4() {
    if (dayT4 > 0.05f) {
        glColor4ub(255, 218, 170, (unsigned char)(90.0f * dayT4));
        glLineWidth(1.0f);
        glBegin(GL_LINES);
            for (int i = 0; i < NUM_LIGHT_SPANS4; i++) {
                const LightSpan4& sp = lightSpans4[i];
                glVertex2f(sp.x1, sp.y1); glVertex2f(sp.x2, sp.y2);
            }
        glEnd();
    }
    for (int i = 0; i < NUM_LIGHT_SPANS4; i++) DrawLightSpan4(lightSpans4[i], i);
}

// ---- Distant skyline ------------------------------------------------------
//  Three rows of town standing behind the market, each one further off than
//  the last. What separates them is not just size: every row is faded toward
//  the air in front of it by AirFade4(), so the back row is pale and low
//  contrast and the front row is dark and crisp. That -- plus the fact that
//  each block shows a receding side face -- is what turns the old flat
//  silhouette band into something with depth in it.
//
//  All of it is day/night aware. By day the blocks are cool grey masonry
//  under a bright sky; by night they are dark but never black, with a
//  scattering of lit windows so the town still reads as inhabited.

// Deterministic "is this window lit" so the pattern is stable frame to frame
// (rand() here would make the whole town flicker) but still looks scattered.
inline bool FarWindowLit4(int seed, int col, int row) {
    int h = seed * 7919 + col * 733 + row * 197;
    h ^= (h >> 7);
    return (h % 100) < 34;
}

// One mass of building. `depth` 0 = right behind the shops, 1 = on the
// horizon. Everything else -- colour, window contrast, whether it gets a
// side face at all -- falls out of that one number.
void DrawFarBlock4(float cx, float w, float h, float depth, float baseY,
                   int seed, bool pitchedRoof) {
    float half = w * 0.5f;
    float topY = baseY + h;

    unsigned char botR, botG, botB, topR, topG, topB;
    AirFade4(depth, baseY, 24, 26, 40,  78,  90, 116, botR, botG, botB);
    AirFade4(depth, topY,  42, 45, 64, 120, 134, 160, topR, topG, topB);

    // ---- Front face: graded, darker at the foot ---------------------------
    glBegin(GL_QUADS);
        glColor3ub(botR, botG, botB);
        glVertex2f(cx - half, baseY); glVertex2f(cx + half, baseY);
        glColor3ub(topR, topG, topB);
        glVertex2f(cx + half, topY);  glVertex2f(cx - half, topY);
    glEnd();

    // ---- Receding side face -----------------------------------------------
    // Thrown toward the vanishing point at x = 0 so the whole row shares one
    // camera instead of each block facing its own way. A block left of centre
    // shows its right flank, one on the right shows its left flank.
    float dir = (cx < 0.0f) ? 1.0f : -1.0f;
    float fx  = cx + dir * half;                       // the corner we see round
    float sw  = fabsf(fx) * 0.048f * (1.0f - depth * 0.55f);
    if (sw > 2.0f) sw = 2.0f;
    if (sw > 0.12f) {
        float k       = sw / (fabsf(fx) + 0.001f);     // fraction of the way to the VP
        float backTop = topY + (baseY - topY) * k;
        // The light sits up and to the right (moon by night, sun by day), so a
        // right-facing flank catches it and a left-facing one is in shadow.
        bool  lit = (dir > 0.0f);
        unsigned char sr, sg, sb;
        if (lit) AirFade4(depth, topY, 34, 37, 52, 120, 130, 152, sr, sg, sb);
        else     AirFade4(depth, topY, 13, 15, 25,  62,  70,  90, sr, sg, sb);
        glColor3ub(sr, sg, sb);
        glBegin(GL_QUADS);
            glVertex2f(fx,          baseY);
            glVertex2f(fx + dir*sw, baseY);
            glVertex2f(fx + dir*sw, backTop);
            glVertex2f(fx,          topY);
        glEnd();
    }

    // ---- Windows -----------------------------------------------------------
    // Skipped on the furthest row: at that distance individual windows would
    // only be noise, and leaving them off is itself a depth cue.
    if (depth < 0.82f && w > 3.0f && h > 3.0f) {
        int cols = (int)(w / 1.9f); if (cols > 7) cols = 7;
        int rows = (int)(h / 2.0f); if (rows > 6) rows = 6;
        if (cols > 0 && rows > 0) {
            float slot = w / (cols + 1.0f);
            float ww   = slot * 0.42f;
            float wh   = (h / (rows + 1.0f)) * 0.44f;
            float fade = 1.0f - depth;                  // far windows lose contrast
            for (int c = 0; c < cols; c++) {
                for (int rw = 0; rw < rows; rw++) {
                    float wx = cx - half + slot * (c + 1) - ww * 0.5f;
                    float wy = baseY + (h / (rows + 1.0f)) * (rw + 1) - wh * 0.5f;
                    bool on = FarWindowLit4(seed, c, rw);
                    if (on) {
                        // Warm rooms, only at night; by day the glass is just
                        // a slightly cooler patch of wall.
                        glColor4ub(MixB4(255, 150), MixB4(202, 168), MixB4(120, 192),
                                   (unsigned char)((MixB4(225, 120)) * (0.35f + 0.65f * fade)));
                    } else {
                        glColor4ub(MixB4(14, 74), MixB4(16, 84), MixB4(26, 104),
                                   (unsigned char)(190 * (0.30f + 0.70f * fade)));
                    }
                    glBegin(GL_QUADS);
                        glVertex2f(wx,      wy);      glVertex2f(wx + ww, wy);
                        glVertex2f(wx + ww, wy + wh); glVertex2f(wx,      wy + wh);
                    glEnd();
                }
            }
        }
    }

    // ---- Roof --------------------------------------------------------------
    if (pitchedRoof) {
        unsigned char rr, rg, rb;
        AirFade4(depth, topY, 46, 40, 58, 118, 122, 140, rr, rg, rb);
        glColor3ub(rr, rg, rb);
        glBegin(GL_TRIANGLES);
            glVertex2f(cx - half - 0.35f, topY);
            glVertex2f(cx + half + 0.35f, topY);
            glVertex2f(cx, topY + 1.1f + w * 0.055f);
        glEnd();
    }
    // Snow cap. Even on the back row it stays visible: a winter skyline with
    // bare roofs against a snowing sky does not hold together.
    unsigned char sc_r, sc_g, sc_b;
    AirFade4(depth * 0.55f, topY, 214, 224, 240, 246, 250, 255, sc_r, sc_g, sc_b);
    float cap = (0.22f + 0.42f * SnowDepth4()) * (1.0f - depth * 0.35f);
    if (pitchedRoof) {
        glColor3ub(sc_r, sc_g, sc_b);
        glBegin(GL_TRIANGLES);
            glVertex2f(cx - half + 0.25f, topY + 0.35f);
            glVertex2f(cx + half - 0.25f, topY + 0.35f);
            glVertex2f(cx, topY + 1.1f + w * 0.055f + cap * 0.6f);
        glEnd();
    } else {
        glColor3ub(sc_r, sc_g, sc_b);
        glBegin(GL_QUADS);
            glVertex2f(cx - half - 0.2f, topY);
            glVertex2f(cx + half + 0.2f, topY);
            glVertex2f(cx + half + 0.1f, topY + cap);
            glVertex2f(cx - half - 0.1f, topY + cap);
        glEnd();
    }

    // Contact shade where the block meets whatever is behind the row in front
    // of it -- stops two rows of flat colour from butting into one shape.
    glColor4ub(MixB4(6, 60), MixB4(8, 68), MixB4(16, 86), (unsigned char)(70 * (1.0f - depth)));
    glBegin(GL_QUADS);
        glVertex2f(cx - half, baseY);
        glVertex2f(cx + half, baseY);
        glVertex2f(cx + half, baseY + 0.9f);
        glVertex2f(cx - half, baseY + 0.9f);
    glEnd();
}

struct FarBlock4 { float x, w, h; bool pitched; };

void DrawDistantBuildings4() {
    // Back row: pale, low contrast, no windows. Deliberately the vaguest
    // thing in the frame.
    static const FarBlock4 rowBack[] = {
        { -54.0f, 15.0f,  7.5f, false }, { -40.0f, 11.0f, 10.5f, false },
        { -29.0f, 10.0f,  6.2f, true  }, { -16.0f, 13.0f,  9.0f, false },
        {  -2.0f, 11.0f, 11.5f, false }, {  11.0f, 12.0f,  7.8f, true  },
        {  24.0f, 13.0f, 12.2f, false }, {  38.0f, 11.0f,  8.6f, false },
        {  51.0f, 15.0f, 10.0f, false }
    };
    // Mid row: a storey lower and a step nearer, so its rooflines break the
    // back row's instead of echoing it.
    static const FarBlock4 rowMid[] = {
        { -57.0f, 12.0f,  5.4f, true  }, { -45.0f,  9.0f,  8.2f, false },
        { -33.5f, 10.0f,  4.6f, true  }, { -21.0f, 11.0f,  7.4f, false },
        {  -9.0f,  9.5f,  9.6f, false }, {   3.0f, 10.0f,  5.2f, true  },
        {  16.0f, 11.0f,  8.8f, false }, {  30.0f,  9.5f,  6.0f, true  },
        {  44.0f, 12.0f,  9.2f, false }, {  57.0f, 10.0f,  6.6f, true  }
    };
    // Front row: nearest, darkest, most detail. It tucks in just behind the
    // shop roofline so the two layers overlap rather than stacking.
    static const FarBlock4 rowNear[] = {
        { -51.0f, 10.0f,  4.0f, true  }, { -37.5f,  8.5f,  6.4f, false },
        { -24.0f,  9.0f,  3.4f, true  }, { -11.0f,  9.5f,  5.6f, false },
        {   2.5f,  8.0f,  3.8f, true  }, {  15.0f,  9.0f,  6.8f, false },
        {  28.0f,  8.5f,  4.2f, true  }, {  41.0f,  9.0f,  5.8f, false },
        {  54.5f, 10.0f,  3.6f, true  }
    };

    const int nBack = (int)(sizeof(rowBack) / sizeof(rowBack[0]));
    const int nMid  = (int)(sizeof(rowMid)  / sizeof(rowMid[0]));
    const int nNear = (int)(sizeof(rowNear) / sizeof(rowNear[0]));

    for (int i = 0; i < nBack; i++)
        DrawFarBlock4(rowBack[i].x, rowBack[i].w, rowBack[i].h, 0.84f, -3.2f, i + 3, rowBack[i].pitched);

    // A band of air between the rows. Very cheap, and it is what stops the
    // three rows reading as one cut-out with notches in it. It has to fade
    // out at BOTH ends: a flat-alpha quad leaves two horizontal seams across
    // the skyline, which reads as a mistake rather than as distance.
    glBegin(GL_QUAD_STRIP);
        glColor4ub(MixB4(46, 208), MixB4(54, 224), MixB4(86, 242), 0);
        glVertex2f(-60.0f, 8.5f); glVertex2f(60.0f, 8.5f);
        glColor4ub(MixB4(46, 208), MixB4(54, 224), MixB4(86, 242), MixB4(56, 96));
        glVertex2f(-60.0f, 1.0f); glVertex2f(60.0f, 1.0f);
        glColor4ub(MixB4(46, 208), MixB4(54, 224), MixB4(86, 242), 0);
        glVertex2f(-60.0f, -5.6f); glVertex2f(60.0f, -5.6f);
    glEnd();

    for (int i = 0; i < nMid; i++)
        DrawFarBlock4(rowMid[i].x, rowMid[i].w, rowMid[i].h, 0.58f, -4.1f, i + 21, rowMid[i].pitched);

    glBegin(GL_QUAD_STRIP);
        glColor4ub(MixB4(44, 204), MixB4(52, 220), MixB4(84, 240), 0);
        glVertex2f(-60.0f, 5.5f); glVertex2f(60.0f, 5.5f);
        glColor4ub(MixB4(44, 204), MixB4(52, 220), MixB4(84, 240), MixB4(38, 66));
        glVertex2f(-60.0f, -0.5f); glVertex2f(60.0f, -0.5f);
        glColor4ub(MixB4(44, 204), MixB4(52, 220), MixB4(84, 240), 0);
        glVertex2f(-60.0f, -5.8f); glVertex2f(60.0f, -5.8f);
    glEnd();

    for (int i = 0; i < nNear; i++)
        DrawFarBlock4(rowNear[i].x, rowNear[i].w, rowNear[i].h, 0.34f, -5.0f, i + 47, rowNear[i].pitched);
}

// ---- Street canyon framing the road ---------------------------------------
//  The road runs out of the frame through the gap between the corner shop and
//  TOYS. A road needs walls or it is only a pale shape painted on the snow, so
//  two runs of facade recede up that gap toward the point the road converges
//  on. They are drawn before the shop row, so the shops crop them cleanly at
//  each side and the whole thing reads as a side street seen end-on.
constexpr float roadFarX4  = -35.4f;   // centre of the road where it meets the horizon
constexpr float roadFarY4  =  -6.0f;   // the plaza horizon: where the road starts
constexpr float roadFarHW4 =   1.70f;  // half-width at that far end

void DrawRoadsideRun4(float nearX, float nearTop, float farX, float farTop,
                      int seed, int nBays) {
    // One run of facade: a wedge from a tall near edge down to a short far
    // edge, cut into bays so it reads as several buildings rather than one
    // ramp. Everything about a bay -- height, colour, window size -- is driven
    // by how far along the wedge it sits, which is what gives the run its
    // perspective.
    for (int i = 0; i < nBays; i++) {
        float t0 = (float)i       / nBays;
        float t1 = (float)(i + 1) / nBays;
        float x0 = nearX + (farX - nearX) * t0;
        float x1 = nearX + (farX - nearX) * t1;
        // Each bay steps down, with a little variation so the ridge is not a
        // perfectly straight diagonal.
        float jitter = sinf((float)(seed + i) * 2.3f) * 0.9f * (1.0f - t0);
        float y0 = nearTop + (farTop - nearTop) * t0 + jitter;
        float y1 = nearTop + (farTop - nearTop) * t1 + jitter;
        float depth = 0.30f + 0.55f * t0;

        unsigned char botR, botG, botB, topR, topG, topB;
        // The canyon wall is turned away from the plaza, so it sits in its own
        // shade: darker than the shop fronts either side of it, which is what
        // makes the gap read as a gap.
        AirFade4(depth, roadFarY4, 27, 29, 42,  86,  94, 114, botR, botG, botB);
        AirFade4(depth, y0,        50, 53, 72, 126, 134, 152, topR, topG, topB);
        glBegin(GL_QUADS);
            glColor3ub(botR, botG, botB);
            glVertex2f(x0, roadFarY4); glVertex2f(x1, roadFarY4);
            glColor3ub(topR, topG, topB);
            glVertex2f(x1, y1);        glVertex2f(x0, y0);
        glEnd();

        // Party wall between bays, catching a sliver of light on its edge.
        unsigned char er, eg, eb;
        AirFade4(depth, y0, 72, 76, 96, 158, 166, 180, er, eg, eb);
        glColor3ub(er, eg, eb);
        glBegin(GL_QUADS);
            glVertex2f(x1 - (x1 - x0) * 0.10f, roadFarY4);
            glVertex2f(x1,                     roadFarY4);
            glVertex2f(x1,                     y1);
            glVertex2f(x1 - (x1 - x0) * 0.10f, y1 + (y0 - y1) * 0.10f);
        glEnd();

        // Windows: two ranks, shrinking and dimming down the run.
        float bayW = fabsf(x1 - x0);
        for (int rw = 0; rw < 2; rw++) {
            for (int c = 0; c < 2; c++) {
                float fx = x0 + (x1 - x0) * (0.24f + 0.44f * c);
                float fy = roadFarY4 + (y0 - roadFarY4) * (0.34f + 0.33f * rw);
                float ww = bayW * 0.16f * (1.0f - t0 * 0.45f);
                float wh = ww * 1.5f;
                if (ww < 0.08f) continue;
                if (FarWindowLit4(seed * 3 + 11, c, rw + i)) {
                    glColor4ub(MixB4(255, 168), MixB4(206, 182), MixB4(126, 202),
                               (unsigned char)(MixB4(250, 150) * (1.0f - t0 * 0.4f)));
                } else {
                    glColor4ub(MixB4(14, 66), MixB4(17, 76), MixB4(30, 98),
                               (unsigned char)(215 * (1.0f - t0 * 0.4f)));
                }
                glBegin(GL_QUADS);
                    glVertex2f(fx - ww, fy - wh); glVertex2f(fx + ww, fy - wh);
                    glVertex2f(fx + ww, fy + wh); glVertex2f(fx - ww, fy + wh);
                glEnd();
            }
        }

        // Snow along the bay's ridge.
        unsigned char sr, sg, sb;
        AirFade4(depth * 0.5f, y0, 210, 220, 238, 244, 248, 255, sr, sg, sb);
        float cap = (0.22f + 0.40f * SnowDepth4()) * (1.0f - t0 * 0.5f);
        glColor3ub(sr, sg, sb);
        glBegin(GL_QUADS);
            glVertex2f(x0, y0); glVertex2f(x1, y1);
            glVertex2f(x1, y1 + cap); glVertex2f(x0, y0 + cap);
        glEnd();
    }
}

void DrawRoadsideBlocks4() {
    // Left-hand wall: runs from the corner shop's near edge down to the point
    // the road vanishes.
    DrawRoadsideRun4(-48.6f, 7.8f, roadFarX4 - roadFarHW4 - 0.2f, -3.0f, 5, 5);
    // Right-hand wall: from TOYS' near edge back to the same point. It is a
    // touch lower, so the two sides do not mirror each other.
    DrawRoadsideRun4(-28.8f, 6.4f, roadFarX4 + roadFarHW4 + 0.2f, -3.2f, 12, 3);

    // Depth haze in the mouth of the street. A stack of soft ellipses read as
    // a grey smudge sitting on the buildings; one wedge, strongest right at
    // the vanishing point and gone by the time it reaches the shopfronts
    // either side, is what actually sells the distance.
    float hr, hg, hb;
    SkyAirColour4(-3.5f, hr, hg, hb);
    unsigned char hzR = (unsigned char)hr, hzG = (unsigned char)hg, hzB = (unsigned char)hb;
    glBegin(GL_TRIANGLE_FAN);
        glColor4ub(hzR, hzG, hzB, MixB4(96, 140));
        glVertex2f(roadFarX4, roadFarY4 + 0.8f);
        glColor4ub(hzR, hzG, hzB, 0);
        for (int i = 0; i <= 20; i++) {
            float a = (float)i / 20.0f * 6.2831853f;
            glVertex2f(roadFarX4 + 5.2f * cosf(a), roadFarY4 + 0.8f + 3.0f * sinf(a));
        }
    glEnd();

    // A far-end lamp glow at night, so the street has a reason to be lit.
    float night = NightT4();
    if (night > 0.02f) {
        DrawSoftEllipse(roadFarX4, roadFarY4 + 1.6f, 2.6f, 1.6f,
                        255, 198, 128, (unsigned char)(78 * night), 4);
    }
}

// ---- Elevated metro rail behind the shops ------------------------------
void DrawMetroRail4() {
    // determine a sensible y above the tallest shop eave so the rail sits
    float maxEave = -999.0f;
    for (int i = 0; i < NUM_SHOPS4; i++) {
        float ey = ShopEaveY4(shops4[i]);
        if (ey > maxEave) maxEave = ey;
    }
    float railY = maxEave + 1.6f;

    // Make the rail visible both by night and by day: keep a base alpha
    float night = NightT4();
    unsigned char beamA = (unsigned char)fminf(255.0f, 90.0f + night * 140.0f);
    // Main elevated beam
    glColor4ub(48, 48, 56, beamA);
    glBegin(GL_QUADS);
        glVertex2f(-62.0f, railY + 0.45f); glVertex2f(62.0f, railY + 0.45f);
        glVertex2f(62.0f, railY - 0.05f);  glVertex2f(-62.0f, railY - 0.05f);
    glEnd();

    // Vertical supports spaced across the width — deeper and thicker for realism
    unsigned char supportA = (unsigned char)fminf(255.0f, 70.0f + night * 150.0f);
    glColor4ub(36, 36, 44, supportA);
    for (float x = -52.0f; x <= 52.0f; x += 8.0f) {
        glBegin(GL_QUADS);
            glVertex2f(x - 0.6f, railY - 0.05f); glVertex2f(x + 0.6f, railY - 0.05f);
            glVertex2f(x + 1.2f, railY - 8.0f);   glVertex2f(x - 1.2f, railY - 8.0f);
        glEnd();
    }

    // Carriage: larger, train-like silhouette with roof and wheels
    unsigned char carA = (unsigned char)fminf(255.0f, 120.0f + night * 120.0f);
    float carHalf = 18.0f; // half-length for the carriage silhouette
    float carTop = railY + 0.9f, carBot = railY - 0.05f;
    float carCenter = metroCarX4; // global position (updated by timer)
    // body
    glColor4ub(80, 78, 90, carA);
    glBegin(GL_QUADS);
        glVertex2f(carCenter - carHalf, carTop); glVertex2f(carCenter + carHalf, carTop);
        glVertex2f(carCenter + carHalf, carBot); glVertex2f(carCenter - carHalf, carBot);
    glEnd();
    // roof cap
    glColor4ub(70, 68, 78, (unsigned char)fminf(220.0f, 90.0f + night * 120.0f));
    glBegin(GL_QUADS);
        glVertex2f(carCenter - carHalf - 0.6f, carTop); glVertex2f(carCenter + carHalf + 0.6f, carTop);
        glVertex2f(carCenter + carHalf + 0.6f, carTop + 0.5f); glVertex2f(carCenter - carHalf - 0.6f, carTop + 0.5f);
    glEnd();
    // windows (brighter in day)
    unsigned char winA = (unsigned char)fminf(255.0f, 140.0f + dayT4 * 80.0f);
    glColor4ub(200, 210, 220, winA);
    float w = 3.2f;
    for (float x = carCenter - carHalf + 2.0f; x < carCenter + carHalf - 1.0f; x += 5.0f) {
        glBegin(GL_QUADS);
            glVertex2f(x, carTop - 0.15f); glVertex2f(x + w, carTop - 0.15f);
            glVertex2f(x + w, carBot + 0.08f); glVertex2f(x, carBot + 0.08f);
        glEnd();
    }
    // wheels removed per user request: skip drawing small circles here
}

void UpdateTwinkle4(int) {
    // The sunrise runs before the pause check: pressing 4 while the scene is
    // paused should still get you to daylight, it just gets there without the
    // snow and the wheel moving.
    if (dayT4 != dayTarget4) {
        const float step = 0.018f;              // ~3.3 s for a full crossfade
        if (dayT4 < dayTarget4) dayT4 = (dayT4 + step > dayTarget4) ? dayTarget4 : dayT4 + step;
        else                    dayT4 = (dayT4 - step < dayTarget4) ? dayTarget4 : dayT4 - step;
        glutPostRedisplay();
    }
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateTwinkle4, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating4) {
        twinklePhase4 += 0.03f;
        if (rand() % 10 == 0) {
            int s = rand() % NUM_LIGHT_SPANS4;
            int b = rand() % BULBS_PER_SPAN4;
            bulbLit4[s][b] = !bulbLit4[s][b];
        }
    }
    glutTimerFunc(60, UpdateTwinkle4, 0);
}

// ============================================================================
//  LIGHT ON THE SNOW
// ----------------------------------------------------------------------------
//  Snow is the most reflective surface in the project and, until now, nothing
//  in this scene lit it. Every lamp, window and bulb was a bright dot sitting
//  on a flat grey field with no relationship to the ground under it -- which
//  is the main reason the plaza read as dead space.
//
//  DrawGroundGlow4 lays a soft pool of light on the paving; DrawLightPools4
//  places one under every source, in one pass, before any prop is drawn.
// ============================================================================
void DrawGroundGlow4(float x, float y, float rx, float ry,
                     unsigned char r, unsigned char g, unsigned char b,
                     unsigned char a) {
    DrawSoftEllipse(x, y, rx, ry, r, g, b, a, 4);
}

void DrawLightPools4() {
    float nightGlow = NightT4();
    // Cool moonlight wash first. The moon sits at (34, 27), so the wash is
    // centred left of it and the whole plaza picks up a faint blue sheen.
    DrawGroundGlow4(18.0f, -18.0f, 62.0f, 17.0f, 150, 178, 225, (unsigned char)(34 * nightGlow));

    // Warm spill from the shopfronts along the back of the plaza
    for (int i = 0; i < NUM_SHOPS4; i++) {
        const Shop4& sh = shops4[i];
        DrawGroundGlow4(sh.x, -7.4f, sh.w * 0.62f, 2.4f, 255, 196, 120, (unsigned char)(62 * nightGlow));
    }

    // Pools under each string-light span, at the low point of its catenary
    for (int i = 0; i < NUM_LIGHT_SPANS4; i++) {
        const LightSpan4& sp = lightSpans4[i];
        float mx = (sp.x1 + sp.x2) * 0.5f;
        unsigned char r = 255, g = 205, b = 140;
        if (multicolorLights4) { r = 225; g = 175; b = 205; }
        DrawGroundGlow4(mx, -8.6f, fabsf(sp.x2 - sp.x1) * 0.42f, 2.8f, r, g, b, (unsigned char)(46 * nightGlow));
    }

    // Market stalls: each awning lamp throws a small warm pool at its feet
    for (int i = 0; i < NUM_STALLS4; i++)
        DrawGroundGlow4(stalls4[i].x, stallGroundY4 - 0.2f,
                        StallHalfW4() + 1.2f, 1.7f, 255, 190, 110, (unsigned char)(88 * nightGlow));

    // The three biggest sources in the scene
    DrawGroundGlow4(ferrisCX4, ferrisBaseY4 + 1.5f, ferrisR4 * 1.25f, 3.8f,
                    multicolorLights4 ? 210 : 255,
                    multicolorLights4 ? 150 : 190,
                    multicolorLights4 ? 220 : 110, (unsigned char)(74 * nightGlow));
    DrawGroundGlow4(treeX4,    -9.2f,  8.0f, 2.3f, 150, 255, 180, (unsigned char)(62 * nightGlow));
    DrawGroundGlow4(clockX4,   -6.9f,  6.5f, 2.0f, 255, 210, 150, (unsigned char)(58 * nightGlow));
    DrawGroundGlow4(-56.5f,    -9.4f,  6.0f, 1.9f, 255, 170, 120, (unsigned char)(54 * nightGlow));

    if (dayT4 > 0.02f) {
        DrawGroundGlow4(8.0f, -16.0f, 48.0f, 12.0f, 232, 220, 180, (unsigned char)(65.0f * dayT4));
    }

    // When the aurora is up it has to reach the ground or it is only a
    // wallpaper effect: a wide, very faint green wash over the whole plaza,
    // rising and falling with the same envelope the curtains use.
    if (auroraActive4) {
        float env = sinf(auroraTimer4 * PI4);
        if (env > 0.0f)
            DrawGroundGlow4(-4.0f, -13.0f, 66.0f, 13.0f, 90, 230, 170,
                            (unsigned char)(30.0f * env * nightGlow));
    }
}

// ============================================================================
//  FERRIS WHEEL
// ============================================================================
// A bulb's colour: warm by default, cycling through four colours when the
// 'L' toggle is on, so the whole fairground responds to one key.
void FerrisBulbColour4(int index, unsigned char& r, unsigned char& g, unsigned char& b) {
    if (multicolorLights4) {
        const unsigned char cols[4][3] = { {255,70,70}, {70,225,110}, {80,160,255}, {255,225,90} };
        int c = index % 4;
        r = cols[c][0]; g = cols[c][1]; b = cols[c][2];
    } else { r = 255; g = 208; b = 135; }
}

constexpr int FERRIS_RIM_BULBS4 = 30;

void DrawFerrisWheel4() {
    float nightGlow = NightT4();
    // ---- Halo -----------------------------------------------------------
    // A fairground wheel at a night market has to glow. This wide, very low
    // alpha disc sits behind the whole structure and lifts it off the sky.
    unsigned char hr, hg, hb;
    FerrisBulbColour4(0, hr, hg, hb);
    DrawSoftEllipse(ferrisCX4, ferrisCY4, ferrisR4 * 1.75f, ferrisR4 * 1.75f,
                    hr, hg, hb, (unsigned char)(34 * nightGlow), 4);

    // ---- A-frame --------------------------------------------------------
    // The legs splay in proportion to the height they carry, so a taller wheel
    // gets a wider, still-plausible stance instead of stilts.
    const float frameH  = ferrisCY4 - ferrisBaseY4;
    const float legHalf = frameH * 0.46f;
    const float braceY  = ferrisBaseY4 + frameH * 0.38f;
    glColor3ub(MixB4(70, 110), MixB4(70, 118), MixB4(80, 120));
    glLineWidth(2.6f * ferrisScale4);
    glBegin(GL_LINES);
        glVertex2f(ferrisCX4-legHalf, ferrisBaseY4); glVertex2f(ferrisCX4, ferrisCY4);
        glVertex2f(ferrisCX4+legHalf, ferrisBaseY4); glVertex2f(ferrisCX4, ferrisCY4);
        glVertex2f(ferrisCX4-legHalf*0.62f, braceY);
        glVertex2f(ferrisCX4+legHalf*0.62f, braceY);   // cross brace
    glEnd();
    // Feet, so the legs land on something rather than stopping in mid-snow
    glLineWidth(3.4f * ferrisScale4);
    glBegin(GL_LINES);
        glVertex2f(ferrisCX4-legHalf-1.0f, ferrisBaseY4);
        glVertex2f(ferrisCX4-legHalf+1.0f, ferrisBaseY4);
        glVertex2f(ferrisCX4+legHalf-1.0f, ferrisBaseY4);
        glVertex2f(ferrisCX4+legHalf+1.0f, ferrisBaseY4);
    glEnd();

    // ---- Rim ------------------------------------------------------------
    glColor3ub(MixB4(150, 135), MixB4(52, 110), MixB4(74, 90));
    glLineWidth(2.4f * ferrisScale4);
    glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 28; i++) {
            float a = (float)i / 28 * 2 * PI4;
            glVertex2f(ferrisCX4 + ferrisR4*cos(a), ferrisCY4 + ferrisR4*sin(a));
        }
    glEnd();

    // Rim bulbs running a chase pattern. The wave travels around the wheel
    // rather than every bulb blinking together, which is what a real ride
    // does and what makes it read as lit rather than decorated.
    for (int i = 0; i < FERRIS_RIM_BULBS4; i++) {
        float a = (float)i / FERRIS_RIM_BULBS4 * 2.0f * PI4 + ferrisAngle4 * 0.25f;
        float bx = ferrisCX4 + ferrisR4 * cosf(a);
        float by = ferrisCY4 + ferrisR4 * sinf(a);
        float chase = 0.35f + 0.65f * (0.5f + 0.5f * sinf(twinklePhase4 * 4.0f - i * 0.62f));
        unsigned char r, g, b;
        FerrisBulbColour4(i, r, g, b);
        // glow, then the filament
        FilledCircle4(bx, by, 0.62f * ferrisScale4 * chase, r, g, b, (unsigned char)(70 * chase * nightGlow));
        FilledCircle4(bx, by, 0.20f * ferrisScale4,
                      (unsigned char)(r * chase * nightGlow), (unsigned char)(g * chase * nightGlow),
                      (unsigned char)(b * chase * nightGlow), (unsigned char)(255 * nightGlow));
    }

    // Bulbs marching down alternate spokes
    for (int sp = 0; sp < NUM_CABINS4; sp++) {
        float a = ferrisAngle4 + sp * (2*PI4/NUM_CABINS4);
        for (int k = 1; k <= 2; k++) {
            float rr = ferrisR4 * (0.38f + 0.30f * k);
            float bx = ferrisCX4 + rr * cosf(a);
            float by = ferrisCY4 + rr * sinf(a);
            float chase = 0.4f + 0.6f * (0.5f + 0.5f * sinf(twinklePhase4 * 3.0f - sp * 0.8f));
            unsigned char r, g, b;
            FerrisBulbColour4(sp + k, r, g, b);
            FilledCircle4(bx, by, 0.16f * ferrisScale4,
                          (unsigned char)(r * chase * nightGlow), (unsigned char)(g * chase * nightGlow),
                          (unsigned char)(b * chase * nightGlow), (unsigned char)(245 * nightGlow));
        }
    }

    for (int i = 0; i < NUM_CABINS4; i++) {
        float a = ferrisAngle4 + i * (2*PI4/NUM_CABINS4);
        float cx = ferrisCX4 + ferrisR4*cosf(a);
        float cy = ferrisCY4 + ferrisR4*sinf(a);
        glColor3ub(MixB4(90, 120), MixB4(90, 120), MixB4(100, 122));
        glLineWidth(1.0f * ferrisScale4);
        glBegin(GL_LINES); glVertex2f(ferrisCX4, ferrisCY4); glVertex2f(cx, cy); glEnd();
        const float g = ferrisScale4;
        glColor3ub(MixB4(70, 90), MixB4(70, 90), MixB4(80, 100));
        glLineWidth(1.2f * g);
        glBegin(GL_LINES);
            glVertex2f(cx, cy + 0.95f*g); glVertex2f(cx, cy + 0.45f*g);
        glEnd();

        unsigned char lit = (i % 2 == 0) ? 235 : 150;
        FilledCircle4(cx, cy, 1.15f*g, 255, 210, 120, (unsigned char)((lit / 5) * nightGlow));

        glColor3ub(MixB4(210, 195), MixB4(70, 80), MixB4(85, 100));
        glBegin(GL_QUADS);
            glVertex2f(cx-0.62f*g, cy-0.52f*g); glVertex2f(cx+0.62f*g, cy-0.52f*g);
            glVertex2f(cx+0.70f*g, cy+0.20f*g); glVertex2f(cx-0.70f*g, cy+0.20f*g);
        glEnd();
        glColor3ub((unsigned char)(255 * nightGlow), (unsigned char)(226 * nightGlow), (unsigned char)(lit * nightGlow));
        glBegin(GL_QUADS);
            glVertex2f(cx-0.52f*g, cy-0.18f*g); glVertex2f(cx+0.52f*g, cy-0.18f*g);
            glVertex2f(cx+0.56f*g, cy+0.18f*g); glVertex2f(cx-0.56f*g, cy+0.18f*g);
        glEnd();
        glColor3ub(240, 240, 245);
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(cx, cy + 0.22f*g);
            for (int k = 0; k <= 8; k++) {
                float a2 = PI4 * ((float)k / 8.0f);
                glVertex2f(cx + 0.72f*g * cosf(a2), cy + 0.22f*g + 0.40f*g * sinf(a2));
            }
        glEnd();
        if (i % 2 == 0) {
            FilledCircle4(cx - 0.20f*g, cy - 0.02f*g, 0.13f*g, 60, 50, 60, 235);
            FilledCircle4(cx + 0.20f*g, cy - 0.04f*g, 0.11f*g, 60, 50, 60, 235);
        }
    }
    FilledCircle4(ferrisCX4, ferrisCY4, 2.10f * ferrisScale4, 255, 225, 170, (unsigned char)(44 * nightGlow));
    FilledCircle4(ferrisCX4, ferrisCY4, 0.90f * ferrisScale4, 255, 236, 196, (unsigned char)(120 * nightGlow));
    FilledCircle4(ferrisCX4, ferrisCY4, 0.50f * ferrisScale4, 70, 66, 76, (unsigned char)(255 * nightGlow));
    FilledCircle4(ferrisCX4, ferrisCY4, 0.22f * ferrisScale4, 255, 246, 220, (unsigned char)(255 * nightGlow));
}

void UpdateFerrisWheel4(int) {
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateFerrisWheel4, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating4) ferrisAngle4 += 0.01f;
    glutTimerFunc(30, UpdateFerrisWheel4, 0);
}

// ============================================================================
//  ICE RINK + SKATERS
// ============================================================================
void DrawRink4() {
    // The rink is ice: out of season it dissolves and DrawFountainAutumn4()
    // takes over the same ground.
    if (SeasonWinter4() < 0.02f) return;
    // Ice sheet, graded so the far edge is paler than the near edge
    glBegin(GL_TRIANGLE_FAN);
        glColor3ub(MixB4(196, 160), MixB4(224, 220), MixB4(242, 245));
        glVertex2f(rinkCX4, rinkCY4);
        glColor3ub(MixB4(168, 140), MixB4(202, 202), MixB4(226, 206));
        for (int i = 0; i <= 30; i++) {
            float a = (float)i / 30 * 2 * PI4;
            glVertex2f(rinkCX4 + 6.0f*cos(a), rinkCY4 + 2.2f*sin(a));
        }
    glEnd();

    // Moon specular: a pale streak lying across the ice from the moon side
    glColor4ub(238, 246, 255, (unsigned char)(90 * NightT4()));
    glBegin(GL_QUADS);
        glVertex2f(rinkCX4 + 1.0f, rinkCY4 + 1.5f);
        glVertex2f(rinkCX4 + 5.2f, rinkCY4 + 0.2f);
        glVertex2f(rinkCX4 + 5.0f, rinkCY4 - 0.3f);
        glVertex2f(rinkCX4 + 0.9f, rinkCY4 + 1.0f);
    glEnd();
    if (dayT4 > 0.08f) {
        glColor4ub(255, 214, 160, (unsigned char)(45 * dayT4));
        glBegin(GL_QUADS);
            glVertex2f(rinkCX4 - 1.0f, rinkCY4 + 1.7f);
            glVertex2f(rinkCX4 + 4.4f, rinkCY4 + 0.5f);
            glVertex2f(rinkCX4 + 4.0f, rinkCY4 - 0.2f);
            glVertex2f(rinkCX4 - 0.8f, rinkCY4 + 1.1f);
        glEnd();
    }

    // Carved skate scratches
    glColor4ub(232, 242, 252, 120);
    glLineWidth(1.0f);
    for (int i = 0; i < 7; i++) {
        float a0 = i * 0.9f;
        glBegin(GL_LINE_STRIP);
            for (int k = 0; k <= 10; k++) {
                float a = a0 + k * 0.16f;
                float rr = 2.2f + (i % 3) * 1.3f;
                glVertex2f(rinkCX4 + rr*cosf(a), rinkCY4 + rr*0.36f*sinf(a));
            }
        glEnd();
    }

    // Boards around the rink, with corner posts and a gate gap at the front
    glColor3ub(226, 230, 238);
    glLineWidth(2.4f);
    glBegin(GL_LINE_STRIP);
        for (int i = 0; i <= 30; i++) {
            float a = -0.35f + (float)i / 30 * (2 * PI4 - 0.70f);
            glVertex2f(rinkCX4 + 6.0f*cos(a), rinkCY4 + 2.2f*sin(a));
        }
    glEnd();
    glColor3ub(150, 60, 65);
    for (int i = 0; i < 6; i++) {
        float a = (float)i / 6 * 2 * PI4 + 0.5f;
        FilledCircle4(rinkCX4 + 6.0f*cos(a), rinkCY4 + 2.2f*sin(a), 0.16f, 150, 60, 65, 255);
    }
    // Gate posts either side of the entrance
    FilledCircle4(rinkCX4 + 6.0f*cosf(-0.35f), rinkCY4 + 2.2f*sinf(-0.35f), 0.20f, 210, 200, 120, 255);
    FilledCircle4(rinkCX4 + 6.0f*cosf(0.35f),  rinkCY4 + 2.2f*sinf(0.35f),  0.20f, 210, 200, 120, 255);
    glLineWidth(1.0f);
}

void DrawSkater4(const Skater4& sk) {
    float x = rinkCX4 + sk.radiusX * cosf(sk.angle);
    float y = rinkCY4 + sk.radiusY * sinf(sk.angle);

    // Is this the one currently on the ice? `fall` is 0 upright, 1 fully down.
    bool  isFaller = (&sk == &skaters4[FALLING_SKATER4]);
    float fall = 0.0f;
    if (isFaller && skaterFallT4 > 0.0f) {
        if      (skaterFallT4 < 0.18f) fall = skaterFallT4 / 0.18f;            // going down
        else if (skaterFallT4 < 0.70f) fall = 1.0f;                            // sat on the ice
        else                            fall = (1.0f - skaterFallT4) / 0.30f;  // getting up
        if (fall < 0.0f) fall = 0.0f;
        if (fall > 1.0f) fall = 1.0f;
    }

    // Lean into the curve: proportional to speed and inverse to radius, so
    // the fast skater on a tight path banks hardest.
    float leanDeg = -cosf(sk.angle) * (sk.speed * 900.0f) / (sk.radiusX + 1.0f);
    if (leanDeg >  22.0f) leanDeg =  22.0f;
    if (leanDeg < -22.0f) leanDeg = -22.0f;

    // Which way they're travelling, for arms and blade orientation
    float dirSign = (-sinf(sk.angle) >= 0.0f) ? 1.0f : -1.0f;

    glPushMatrix();
    glTranslatef(x, y, 0.0f);
    // A fall tips the whole figure over and drops it toward the ice.
    glRotatef(leanDeg + fall * 72.0f * dirSign, 0.0f, 0.0f, 1.0f);
    glScalef(1.0f, 1.0f - fall * 0.35f, 1.0f);

    // Spray of ice kicked up at the moment of the fall
    if (fall > 0.0f && skaterFallT4 < 0.30f) {
        for (int i = 0; i < 5; i++) {
            float t = fmodf(skaterFallT4 * 3.0f + i * 0.2f, 1.0f);
            FilledCircle4(-0.6f * dirSign - t * 1.4f, 0.15f + t * 0.8f,
                          0.16f * (1.0f - t), 244, 250, 255,
                          (unsigned char)(190 * (1.0f - t)));
        }
    }

    // Blade: a bright line at the ice contact point
    glColor3ub(238, 246, 255);
    glLineWidth(1.8f);
    glBegin(GL_LINES);
        glVertex2f(-0.30f * dirSign, 0.02f); glVertex2f(0.34f * dirSign, 0.02f);
    glEnd();

    // Legs
    glColor3ub(40, 40, 52);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        glVertex2f(0.0f, 1.0f); glVertex2f(-0.14f * dirSign, 0.05f);
        glVertex2f(0.0f, 1.0f); glVertex2f( 0.20f * dirSign, 0.05f);
    glEnd();

    // Torso
    glColor3ub(sk.r, sk.g, sk.b);
    glLineWidth(3.5f);
    glBegin(GL_LINES); glVertex2f(0.0f, 1.0f); glVertex2f(0.0f, 1.9f); glEnd();

    // Arms out for balance, swinging opposite the stride
    float swing = sinf(sk.angle * 6.0f) * 0.22f;
    glLineWidth(1.8f);
    glBegin(GL_LINES);
        glVertex2f(0.0f, 1.70f); glVertex2f( 0.62f * dirSign, 1.55f + swing);
        glVertex2f(0.0f, 1.70f); glVertex2f(-0.58f * dirSign, 1.60f - swing);
    glEnd();

    // Head with a bobble hat
    FilledCircle4(0.0f, 2.20f, 0.28f, 225, 185, 145, 255);
    FilledCircle4(0.0f, 2.44f, 0.22f, sk.r, sk.g, sk.b, 255);
    FilledCircle4(0.0f, 2.64f, 0.09f, 250, 250, 255, 255);

    // Scarf trailing behind
    glColor3ub(sk.r, sk.g, sk.b);
    glLineWidth(2.2f);
    glBegin(GL_LINE_STRIP);
        glVertex2f(0.0f, 1.95f);
        glVertex2f(-0.45f * dirSign, 1.80f + swing * 0.5f);
        glVertex2f(-0.85f * dirSign, 1.62f - swing * 0.4f);
    glEnd();

    glLineWidth(1.0f);
    glPopMatrix();
}

void DrawSkaters4() {
    if (SeasonWinter4() < 0.02f) return;   // no ice, no skaters
    for (int i = 0; i < NUM_SKATERS4; i++) DrawSkater4(skaters4[i]);

    // Joined hands between the pair, drawn after both so the arm sits on top.
    const Skater4& a = skaters4[3];
    const Skater4& b = skaters4[4];
    float ax = rinkCX4 + a.radiusX * cosf(a.angle), ay = rinkCY4 + a.radiusY * sinf(a.angle);
    float bx = rinkCX4 + b.radiusX * cosf(b.angle), by = rinkCY4 + b.radiusY * sinf(b.angle);
    glColor3ub(232, 196, 158);
    glLineWidth(2.0f);
    glBegin(GL_LINE_STRIP);
        glVertex2f(ax, ay + 1.68f);
        glVertex2f((ax + bx) * 0.5f, (ay + by) * 0.5f + 1.48f);   // hands dip between them
        glVertex2f(bx, by + 1.68f);
    glEnd();
    glLineWidth(1.0f);
}

void UpdateSkaters4(int) {
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateSkaters4, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating4) {
        for (int i = 0; i < NUM_SKATERS4; i++) {
            // Someone sprawled on the ice is not also gliding.
            float slow = (i == FALLING_SKATER4 && skaterFallT4 > 0.0f)
                       ? (1.0f - 0.92f * (skaterFallT4 < 0.70f ? 1.0f : 0.4f)) : 1.0f;
            skaters4[i].angle += skaters4[i].speed * slow;
        }

        if (skaterFallT4 > 0.0f) {
            skaterFallT4 += 0.006f;
            if (skaterFallT4 >= 1.0f) { skaterFallT4 = 0.0f; skaterFallCool4 = 420 + rand() % 500; }
        } else if (--skaterFallCool4 <= 0) {
            skaterFallT4 = 0.001f;
        }
    }
    glutTimerFunc(25, UpdateSkaters4, 0);
}

// ============================================================================
//  HORSE-DRAWN SLED
// ============================================================================
// ---- Horse: body, neck, head, gallop cycle, breath ----------------------
void DrawHorse4(float x, float y, float gait) {
    float legA = sinf(gait), legB = sinf(gait + PI4);

    // Hind and fore legs, alternating pairs
    glColor3ub(58, 40, 30);
    glLineWidth(2.2f);
    glBegin(GL_LINES);
        glVertex2f(x - 0.75f, y + 0.55f); glVertex2f(x - 0.75f + 0.34f*legA, y - 0.35f);
        glVertex2f(x - 0.55f, y + 0.55f); glVertex2f(x - 0.55f + 0.30f*legB, y - 0.35f);
        glVertex2f(x + 0.70f, y + 0.55f); glVertex2f(x + 0.70f + 0.34f*legB, y - 0.35f);
        glVertex2f(x + 0.50f, y + 0.55f); glVertex2f(x + 0.50f + 0.30f*legA, y - 0.35f);
    glEnd();

    // Barrel
    glColor3ub(78, 54, 38);
    glBegin(GL_POLYGON);
        glVertex2f(x - 0.95f, y + 0.55f);
        glVertex2f(x + 0.95f, y + 0.55f);
        glVertex2f(x + 1.00f, y + 1.30f);
        glVertex2f(x - 0.90f, y + 1.35f);
    glEnd();

    // Neck and head
    glBegin(GL_POLYGON);
        glVertex2f(x + 0.80f, y + 1.10f);
        glVertex2f(x + 1.15f, y + 1.15f);
        glVertex2f(x + 1.70f, y + 2.05f);
        glVertex2f(x + 1.35f, y + 2.10f);
    glEnd();
    glBegin(GL_POLYGON);
        glVertex2f(x + 1.34f, y + 1.98f);
        glVertex2f(x + 1.95f, y + 2.12f);
        glVertex2f(x + 1.98f, y + 2.40f);
        glVertex2f(x + 1.36f, y + 2.34f);
    glEnd();
    // Ear + mane
    glColor3ub(44, 30, 22);
    glBegin(GL_TRIANGLES);
        glVertex2f(x + 1.42f, y + 2.34f); glVertex2f(x + 1.56f, y + 2.34f); glVertex2f(x + 1.48f, y + 2.62f);
    glEnd();
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        glVertex2f(x + 1.30f, y + 2.05f); glVertex2f(x + 0.95f, y + 1.35f);
    glEnd();
    // Tail
    glBegin(GL_LINES);
        glVertex2f(x - 0.92f, y + 1.25f); glVertex2f(x - 1.45f, y + 0.62f + 0.12f*legA);
    glEnd();

    // Harness strap + trace line back toward the sled
    glColor3ub(120, 80, 45);
    glLineWidth(1.6f);
    glBegin(GL_LINES);
        glVertex2f(x + 0.85f, y + 1.20f); glVertex2f(x + 0.85f, y + 0.55f);
        glVertex2f(x - 0.95f, y + 0.95f); glVertex2f(x - 2.10f, y + 0.80f);
    glEnd();

    // Breath puffs from the nostrils in the cold
    for (int i = 0; i < 3; i++) {
        float t = fmodf(firePhase4 * 0.30f + i * 0.33f, 1.0f);
        FilledCircle4(x + 2.05f + t * 1.5f, y + 2.25f + t * 0.35f,
                      0.12f + t * 0.20f, 225, 232, 240,
                      (unsigned char)(120 * (1.0f - t)));
    }
    glLineWidth(1.0f);
}

void DrawSled4() {
    if (!sledActive4) return;
    if (SeasonWinter4() < 0.02f) return;   // a sled needs snow to run on
    float x = sledX4, y = -11.0f;
    float gait = sledX4 * 1.6f;   // legs cycle with distance travelled

    // Horse out in front, pulling
    DrawHorse4(x + 3.4f, y, gait);

    // Sled body
    glColor3ub(112, 72, 46);
    glBegin(GL_QUADS);
        glVertex2f(x-1.3f, y+0.15f);  glVertex2f(x+1.5f, y+0.15f);
        glVertex2f(x+1.5f, y+1.25f);  glVertex2f(x-1.3f, y+1.25f);
    glEnd();
    // Curled front rail
    glColor3ub(140, 92, 58);
    glLineWidth(2.2f);
    glBegin(GL_LINE_STRIP);
        glVertex2f(x+1.5f, y+1.25f); glVertex2f(x+1.9f, y+1.15f); glVertex2f(x+2.0f, y+0.70f);
    glEnd();
    // Runners
    glColor3ub(196, 202, 214);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        glVertex2f(x-1.5f, y+0.05f); glVertex2f(x+1.9f, y+0.05f);
        glVertex2f(x-1.2f, y+0.15f); glVertex2f(x-1.2f, y+0.05f);
        glVertex2f(x+1.3f, y+0.15f); glVertex2f(x+1.3f, y+0.05f);
    glEnd();

    // Two riders under a blanket
    glColor3ub(150, 40, 50);
    glBegin(GL_QUADS);
        glVertex2f(x-1.1f, y+1.25f); glVertex2f(x+1.0f, y+1.25f);
        glVertex2f(x+1.0f, y+1.70f); glVertex2f(x-1.1f, y+1.70f);
    glEnd();
    FilledCircle4(x-0.45f, y+2.00f, 0.30f, 232, 194, 154, 255);
    FilledCircle4(x+0.45f, y+1.95f, 0.27f, 224, 186, 146, 255);
    // Driver's hat
    glColor3ub(40, 40, 50);
    glBegin(GL_TRIANGLES);
        glVertex2f(x-0.72f, y+2.22f); glVertex2f(x-0.18f, y+2.22f); glVertex2f(x-0.45f, y+2.62f);
    glEnd();
    // Reins running forward to the harness
    glColor3ub(90, 62, 38);
    glLineWidth(1.2f);
    glBegin(GL_LINES);
        glVertex2f(x-0.20f, y+1.95f); glVertex2f(x+4.25f, y+1.20f);
    glEnd();

    // Snow spray kicked up by the runners
    for (int i = 0; i < 5; i++) {
        float t = fmodf(firePhase4 * 0.5f + i * 0.2f, 1.0f);
        FilledCircle4(x - 1.6f - t * 1.8f, y + 0.08f + t * 0.5f,
                      0.14f * (1.0f - t), 245, 248, 255,
                      (unsigned char)(170 * (1.0f - t)));
    }
    glLineWidth(1.0f);
}

void UpdateSled4(int) {
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateSled4, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating4) {
        if (sledActive4) {
            sledX4 += 0.4f;
            if (sledX4 > 70.0f) { sledActive4 = false; sledCooldown4 = 300 + rand() % 200; }
        } else {
            sledCooldown4--;
            if (sledCooldown4 <= 0) { sledActive4 = true; sledX4 = -75.0f; }
        }
    }
    glutTimerFunc(25, UpdateSled4, 0);
}

// ============================================================================
//  FIRE PIT / MARKET STALLS / SMOKE + STEAM
// ============================================================================
// ---- Fire pit: stones, logs, layered flame, embers, ground bounce --------
constexpr int MAX_EMBERS4 = 14;
struct Ember4 { float x, y, vy, drift, life; bool active; };
Ember4 embers4[MAX_EMBERS4];

void DrawFirePit4() {
    float flick = 0.72f + 0.28f * sinf(firePhase4 * 3.0f);

    // Warm light bouncing off the snow around the pit, drawn first so
    // everything else sits on top of it.
    glBegin(GL_TRIANGLE_FAN);
        glColor4ub(255, 145, 45, (unsigned char)(95 * flick));
        glVertex2f(fireX4, fireY4);
        glColor4ub(255, 120, 40, 0);
        for (int i = 0; i <= 26; i++) {
            float a = (float)i / 26.0f * 2.0f * PI4;
            glVertex2f(fireX4 + 6.2f * cosf(a), fireY4 + 2.4f * sinf(a));
        }
    glEnd();

    // Stone ring
    for (int i = -3; i <= 3; i++) {
        float sx = fireX4 + i * 0.45f;
        FilledCircle4(sx, fireY4 - 0.25f, 0.26f, 96, 96, 104, 255);
        FilledCircle4(sx, fireY4 - 0.20f, 0.18f, 122, 122, 130, 255);
    }

    // Crossed logs
    glColor3ub(96, 64, 40);
    glLineWidth(4.0f);
    glBegin(GL_LINES);
        glVertex2f(fireX4 - 1.0f, fireY4 - 0.05f); glVertex2f(fireX4 + 0.9f, fireY4 + 0.35f);
        glVertex2f(fireX4 + 1.0f, fireY4 - 0.05f); glVertex2f(fireX4 - 0.9f, fireY4 + 0.35f);
    glEnd();

    // Flame: five tapering tongues, each on its own sine phase, hottest and
    // palest at the core. Replaces the old two overlapping circles.
    struct FlameLayer { float w, h, ph; unsigned char r, g, b, a; };
    FlameLayer layers[5] = {
        { 1.05f, 2.35f, 0.0f, 235,  70,  25, 205 },
        { 0.82f, 2.00f, 1.1f, 255, 120,  35, 220 },
        { 0.60f, 1.62f, 2.2f, 255, 170,  55, 232 },
        { 0.40f, 1.20f, 3.3f, 255, 215, 105, 240 },
        { 0.22f, 0.78f, 4.4f, 255, 246, 205, 250 }
    };
    for (int i = 0; i < 5; i++) {
        FlameLayer& L = layers[i];
        float wob = sinf(firePhase4 * 4.0f + L.ph) * 0.16f;
        float h   = L.h * (0.85f + 0.15f * sinf(firePhase4 * 5.0f + L.ph));
        glColor4ub(L.r, L.g, L.b, L.a);
        glBegin(GL_TRIANGLES);
            glVertex2f(fireX4 - L.w, fireY4 + 0.15f);
            glVertex2f(fireX4 + L.w, fireY4 + 0.15f);
            glVertex2f(fireX4 + wob, fireY4 + 0.15f + h);
        glEnd();
    }

    // Embers drifting up out of the flame
    for (int i = 0; i < MAX_EMBERS4; i++) {
        if (!embers4[i].active) continue;
        unsigned char a = (unsigned char)(230 * embers4[i].life);
        FilledCircle4(embers4[i].x, embers4[i].y, 0.09f * embers4[i].life,
                      255, 180, 80, a);
    }

    // Two people warming their hands at the pit
    for (int s = -1; s <= 1; s += 2) {
        float px = fireX4 + s * 2.6f;
        FilledCircle4(px, fireY4 + 1.55f, 0.28f, 228, 190, 150, 255);
        glColor3ub(s < 0 ? 150 : 60, s < 0 ? 50 : 90, s < 0 ? 60 : 140);
        glBegin(GL_QUADS);
            glVertex2f(px - 0.26f, fireY4 + 0.35f); glVertex2f(px + 0.26f, fireY4 + 0.35f);
            glVertex2f(px + 0.22f, fireY4 + 1.30f); glVertex2f(px - 0.22f, fireY4 + 1.30f);
        glEnd();
        // Arms reaching toward the warmth
        glColor3ub(228, 190, 150);
        glLineWidth(2.0f);
        glBegin(GL_LINES);
            glVertex2f(px - s * 0.22f, fireY4 + 1.05f);
            glVertex2f(px - s * 1.05f, fireY4 + 0.80f);
        glEnd();
    }
    glLineWidth(1.0f);
}

void UpdateFire4(int) {
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateFire4, 0); return; }
    if (isAnimating4) {
        firePhase4 += 0.15f;
        // Spawn embers
        static int acc = 0;
        if (++acc > 5) {
            acc = 0;
            for (int i = 0; i < MAX_EMBERS4; i++) {
                if (!embers4[i].active) {
                    embers4[i].active = true;
                    embers4[i].x = fireX4 + (rand() % 100 - 50) / 90.0f;
                    embers4[i].y = fireY4 + 1.1f;
                    embers4[i].vy = 0.07f + (rand() % 40) / 1000.0f;
                    embers4[i].drift = (rand() % 100 - 50) / 2600.0f;
                    embers4[i].life = 1.0f;
                    break;
                }
            }
        }
        for (int i = 0; i < MAX_EMBERS4; i++) {
            if (!embers4[i].active) continue;
            embers4[i].y += embers4[i].vy;
            embers4[i].x += embers4[i].drift;
            embers4[i].life -= 0.012f;
            if (embers4[i].life <= 0.0f) embers4[i].active = false;
        }
    }
    glutTimerFunc(30, UpdateFire4, 0);
}

// A market stall: striped awning, a named signboard, a vendor behind the
// counter and goods on it. Two of the four stalls used to be blank boxes
// with no sign, no vendor and nothing for sale.
// The body below was drawn against a 1.0 scale, so rather than rescale forty
// vertex literals by hand it is wrapped in one transform anchored at the foot
// of the stall -- the stall then grows upward and outward from the snow it
// stands on. The name board is drawn afterwards, outside the transform,
// because glutBitmapCharacter is sized in pixels and would not scale with it.
void DrawStall4(const Stall4& st) {
    const float x  = st.x;
    const unsigned char ar = st.r, ag = st.g, ab = st.b;
    const char* sign = st.sign;
    const int   goods = st.goods;
    float y = -9.5f;

    BeginDepthSprite(x, y - 2.0f, stallScale4);

    // Counter
    glColor3ub(120, 85, 55);
    glBegin(GL_QUADS);
        glVertex2f(x-2.2f, y-2.0f); glVertex2f(x+2.2f, y-2.0f);
        glVertex2f(x+2.2f, y);      glVertex2f(x-2.2f, y);
    glEnd();
    glColor4ub(86, 60, 38, 190);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        for (int i = 1; i < 5; i++) {
            float px = x - 2.2f + i * 0.88f;
            glVertex2f(px, y-2.0f); glVertex2f(px, y);
        }
    glEnd();

    // Vendor behind the counter, rocking gently on their feet
    float rock = sinf(pedTimer4 * 0.8f + x) * 0.10f;
    glColor3ub(70, 60, 88);
    glLineWidth(5.0f);
    glBegin(GL_LINES); glVertex2f(x - 0.6f + rock, y + 0.1f); glVertex2f(x - 0.6f + rock, y + 1.3f); glEnd();
    FilledCircle4(x - 0.6f + rock, y + 1.65f, 0.32f, 228, 190, 152, 255);
    FilledCircle4(x - 0.6f + rock, y + 1.92f, 0.30f, ar, ag, ab, 255);      // matching hat
    glLineWidth(1.0f);

    // Goods on the counter: 0 = mugs, 1 = wreaths, 2 = roasted chestnuts,
    // 3 = wrapped toys
    for (int i = 0; i < 4; i++) {
        float gx = x - 1.5f + i * 1.0f;
        if (goods == 0) {                                   // steaming mugs
            glColor3ub(240, 240, 245);
            glBegin(GL_QUADS);
                glVertex2f(gx-0.20f, y); glVertex2f(gx+0.20f, y);
                glVertex2f(gx+0.20f, y+0.42f); glVertex2f(gx-0.20f, y+0.42f);
            glEnd();
            float t = fmodf(firePhase4 * 0.35f + i * 0.25f, 1.0f);
            FilledCircle4(gx, y + 0.5f + t * 0.9f, 0.10f + t * 0.16f,
                          235, 240, 248, (unsigned char)(120 * (1.0f - t)));
        } else if (goods == 1) {                            // wreaths
            glColor3ub(40, 110, 62);
            glLineWidth(3.0f);
            glBegin(GL_LINE_LOOP);
                for (int k = 0; k < 10; k++) {
                    float a = (float)k / 10.0f * 2.0f * PI4;
                    glVertex2f(gx + 0.26f*cosf(a), y + 0.30f + 0.26f*sinf(a));
                }
            glEnd();
            FilledCircle4(gx + 0.20f, y + 0.12f, 0.08f, 210, 60, 60, 255);
            glLineWidth(1.0f);
        } else if (goods == 2) {                            // chestnut pan
            if (i == 0) {
                glColor3ub(48, 46, 52);
                glBegin(GL_QUADS);
                    glVertex2f(x-1.7f, y); glVertex2f(x+1.1f, y);
                    glVertex2f(x+1.1f, y+0.34f); glVertex2f(x-1.7f, y+0.34f);
                glEnd();
                for (int k = 0; k < 7; k++)
                    FilledCircle4(x - 1.5f + k * 0.40f, y + 0.40f, 0.15f, 128, 78, 44, 255);
                float t = fmodf(firePhase4 * 0.3f, 1.0f);
                FilledCircle4(x - 0.2f, y + 0.8f + t * 1.1f, 0.18f + t * 0.22f,
                              228, 214, 200, (unsigned char)(110 * (1.0f - t)));
            }
        } else {                                            // wrapped toys
            unsigned char cols[4][3] = { {210,70,80}, {70,140,210}, {230,190,70}, {140,90,190} };
            glColor3ub(cols[i][0], cols[i][1], cols[i][2]);
            glBegin(GL_QUADS);
                glVertex2f(gx-0.26f, y); glVertex2f(gx+0.26f, y);
                glVertex2f(gx+0.26f, y+0.5f); glVertex2f(gx-0.26f, y+0.5f);
            glEnd();
            glColor3ub(250, 250, 250);
            glBegin(GL_LINES);
                glVertex2f(gx, y); glVertex2f(gx, y+0.5f);
            glEnd();
        }
    }

    // Striped awning
    for (int i = 0; i < 6; i++) {
        float t0 = (float)i / 6.0f, t1 = (float)(i+1) / 6.0f;
        bool pale = (i % 2 == 0);
        glColor3ub(pale ? 246 : ar, pale ? 246 : ag, pale ? 250 : ab);
        glBegin(GL_TRIANGLES);
            glVertex2f(x - 2.6f + 5.2f*t0, y);
            glVertex2f(x - 2.6f + 5.2f*t1, y);
            glVertex2f(x, y + 1.6f);
        glEnd();
    }
    // Scalloped fringe
    glColor3ub(ar, ag, ab);
    for (int i = 0; i < 7; i++)
        FilledCircle4(x - 2.5f + i * 0.85f, y - 0.05f, 0.22f, ar, ag, ab, 255);

    // Awning lamp and the warm patch it throws on the counter
    FilledCircle4(x, y + 1.05f, 0.9f, 255, 210, 140, 50);
    FilledCircle4(x, y + 1.05f, 0.22f, 255, 244, 205, 255);

    EndDepthSprite();

    // ---- Name board -------------------------------------------------------
    // Drawn in world space above the scaled canopy. Its width comes from the
    // name rather than a fixed 1.9, which is what let "CHESTNUTS" and
    // "WREATHS" run off both ends of their boards; now that the stall itself
    // is 1.55x, a board sized to its text no longer dwarfs the tent under it.
    if (sign != nullptr) {
        float apex  = StallApexY4();
        // Sitting the board clear above the ridge left it floating: the canopy
        // comes to a point, so there was nothing under most of its width. It
        // is dropped far enough to bite into the peak, and carried on two
        // posts that stand on the slopes, but stays above the awning lamp at
        // StallCounterTopY4() + 1.05*stallScale4 so it cannot black it out.
        float signH = 1.70f;
        float ty    = apex - 0.35f;
        float txtW  = TextPixelWidth(GLUT_BITMAP_HELVETICA_12, sign)
                    * (WORLD_RIGHT - WORLD_LEFT) / (float)viewportPixelWidth;
        float half  = txtW * 0.5f + 0.8f;

        // Posts, drawn before the board so they run behind it
        float eave   = StallCounterTopY4();
        float postX  = half * 0.5f;
        float slopeY = eave + (apex - eave) * (1.0f - postX / StallHalfW4());
        glColor3ub(78, 58, 42);
        glLineWidth(2.6f);
        glBegin(GL_LINES);
            glVertex2f(x-postX, slopeY); glVertex2f(x-postX, ty+signH*0.5f);
            glVertex2f(x+postX, slopeY); glVertex2f(x+postX, ty+signH*0.5f);
        glEnd();
        glLineWidth(1.0f);

        glColor4ub(255, 198, 112, 48);          // the board catches the lamp below it
        glBegin(GL_QUADS);
            glVertex2f(x-half-1.1f, ty-0.9f);       glVertex2f(x+half+1.1f, ty-0.9f);
            glVertex2f(x+half+1.1f, ty+signH+0.9f); glVertex2f(x-half-1.1f, ty+signH+0.9f);
        glEnd();
        glColor3ub(48, 36, 28);
        glBegin(GL_QUADS);
            glVertex2f(x-half, ty);        glVertex2f(x+half, ty);
            glVertex2f(x+half, ty+signH);  glVertex2f(x-half, ty+signH);
        glEnd();
        glColor3ub(198, 156, 96);
        glLineWidth(1.5f);
        glBegin(GL_LINE_LOOP);
            glVertex2f(x-half, ty);        glVertex2f(x+half, ty);
            glVertex2f(x+half, ty+signH);  glVertex2f(x-half, ty+signH);
        glEnd();
        glLineWidth(1.0f);
        glColor3ub(255, 230, 176);
        DrawTextCentered(x, ty + signH*0.5f - 0.45f, GLUT_BITMAP_HELVETICA_12, sign);
    }
}

// A short queue waiting at a stall, so the market looks used rather than set up.
// A short queue waiting at a stall, so the market looks used rather than set
// up. These are built to DrawPerson4's proportions -- feet on the ground,
// hips at +1.0, hat at +2.2 above that -- because the old build topped out at
// 2.2 overall and left the customers a head shorter than the crowd walking
// past them.
void DrawStallQueue4(float x, int n) {
    for (int i = 0; i < n; i++) {
        float qx  = x - StallHalfW4() - 1.1f - i * 1.9f;
        float bob = sinf(pedTimer4 * 1.1f + i * 1.7f) * 0.09f;
        float fy  = stallGroundY4 + bob;
        float hipY = fy + 1.0f;
        unsigned char cols[3][3] = { {150, 60, 70}, {50, 80, 130}, {90, 110, 70} };
        unsigned char cr = cols[i%3][0], cg = cols[i%3][1], cb = cols[i%3][2];

        DrawGroundShadow(qx, stallGroundY4, 0.62f, 0.25f, 80);
        glColor3ub(30, 30, 35);                       // legs
        glLineWidth(2.5f);
        glBegin(GL_LINES);
            glVertex2f(qx, hipY); glVertex2f(qx-0.16f, fy);
            glVertex2f(qx, hipY); glVertex2f(qx+0.16f, fy);
        glEnd();
        glColor3ub(cr, cg, cb);                       // coat
        glLineWidth(5.0f);
        glBegin(GL_LINES); glVertex2f(qx, hipY); glVertex2f(qx, hipY+1.15f); glEnd();
        FilledCircle4(qx, hipY+1.45f, 0.30f, 226, 188, 150, 255);
        glColor3ub(cr, cg, cb);                       // hat
        glBegin(GL_TRIANGLES);
            glVertex2f(qx-0.30f, hipY+1.65f); glVertex2f(qx+0.30f, hipY+1.65f);
            glVertex2f(qx,       hipY+2.20f);
        glEnd();
        glColor4ub(246, 249, 255, 230);               // snow settling on it
        glBegin(GL_TRIANGLES);
            glVertex2f(qx-0.16f*SnowDepth4(), hipY+1.95f);
            glVertex2f(qx+0.16f*SnowDepth4(), hipY+1.95f);
            glVertex2f(qx,                  hipY+2.20f);
        glEnd();
        // Breath, drifting toward the counter they are waiting at
        for (int k = 0; k < 2; k++) {
            float t = fmodf(firePhase4 * 0.25f + k * 0.5f + i * 0.2f, 1.0f);
            FilledCircle4(qx + 0.35f + t * 0.9f, hipY + 1.40f + t * 0.35f,
                          0.10f + t * 0.18f, 226, 234, 244,
                          (unsigned char)(95 * (1.0f - t)));
        }
        glLineWidth(1.0f);
    }
}

void DrawStalls4() {
    for (int i = 0; i < NUM_STALLS4; i++) DrawStall4(stalls4[i]);
    // Both queues form on the stall's left, into open paving rather than back
    // under the ferris wheel, so the customers stay visible.
    DrawStallQueue4(stalls4[1].x, 2);
    DrawStallQueue4(stalls4[3].x, 2);
}

// ---- Christmas tree centerpiece, with twinkling ornaments ------------------
// In autumn this is the same tree in the same place, but the needles turn and
// the baubles become paper lanterns -- the market stays, it is just a
// different month.
void DrawChristmasTree4() {
    float x = treeX4, baseY = -9.0f;
    glColor3ub(80, 55, 35);
    glBegin(GL_QUADS);
        glVertex2f(x-0.4f, baseY-0.5f); glVertex2f(x+0.4f, baseY-0.5f);
        glVertex2f(x+0.4f, baseY);      glVertex2f(x-0.4f, baseY);
    glEnd();
    // Needles turn from winter green to a burnt autumn gold. Each tier is
    // shaded slightly differently so the cone has some form to it.
    for (int tier = 0; tier < 4; tier++) {
        float ty = baseY + tier * 1.8f;
        float tw = 4.2f - tier * 0.85f;
        int lift = tier * 6;
        glColor3ub(MixSB4(30 + lift, 168 + lift), MixSB4(90 + lift, 96 + lift),
                   MixSB4(55 + lift, 44 + lift));
        glBegin(GL_TRIANGLES);
            glVertex2f(x-tw, ty); glVertex2f(x+tw, ty); glVertex2f(x, ty+2.3f);
        glEnd();
    }
    // Snow settled on each tier -- melts out of season with everything else
    if (SeasonWinter4() > 0.02f) {
        float c = SnowDepth4();
        glColor4ub(246, 249, 255, WinterA4(225));
        for (int tier = 0; tier < 4; tier++) {
            float ty = baseY + tier * 1.8f;
            float tw = (4.2f - tier * 0.85f) * (0.55f + 0.35f * c);
            glBegin(GL_TRIANGLES);
                glVertex2f(x - tw, ty + 0.15f);
                glVertex2f(x + tw, ty + 0.15f);
                glVertex2f(x,      ty + 0.15f + 0.75f * (0.5f + 0.5f * c));
            glEnd();
        }
    }
    // Star in winter, a simple finial in autumn
    FilledCircle4(x, baseY+7.7f, 0.35f, MixSB4(255, 210), MixSB4(220, 170), MixSB4(80, 96), 255);

    float ornX[12] = { -2.5f, 1.8f, -1.0f, 2.6f, -3.2f, 0.5f, -1.6f, 2.0f, -0.6f, 1.2f, -2.0f, 0.2f };
    float ornY[12] = {  0.6f, 1.0f,  1.8f, 2.2f,  2.8f, 3.2f,  3.9f, 4.3f,  4.9f, 5.3f,  5.9f, 6.3f };
    // The 'L' toggle reaches the tree too, so one key shifts the mood of the
    // whole plaza rather than only the string lights.
    unsigned char warm[3][3] = { {255, 196, 120}, {255, 214, 150}, {255, 176,  96} };
    unsigned char many[3][3] = { {230,  60,  60}, { 60, 140, 230}, {240, 210,  60} };
    for (int i = 0; i < 12; i++) {
        float tw = 0.5f + 0.5f * sinf(twinklePhase4*2.0f + i*0.7f);
        int c = i % 3;
        unsigned char r = multicolorLights4 ? many[c][0] : warm[c][0];
        unsigned char g = multicolorLights4 ? many[c][1] : warm[c][1];
        unsigned char b = multicolorLights4 ? many[c][2] : warm[c][2];
        // Halo first, then the bauble, so each ornament reads as a light.
        FilledCircle4(x+ornX[i], baseY+ornY[i], 0.75f, r, g, b, (unsigned char)(60 * tw));
        FilledCircle4(x+ornX[i], baseY+ornY[i], 0.22f, r, g, b, (unsigned char)(150 + 105*tw));

        // In autumn the baubles hang as little paper lanterns instead: the
        // same lights, dressed for a different festival.
        if (SeasonAutumn4() > 0.02f) {
            unsigned char a = AutumnA4(235);
            glColor4ub(226, 138, 62, a);
            glBegin(GL_QUADS);
                glVertex2f(x+ornX[i]-0.24f, baseY+ornY[i]-0.02f);
                glVertex2f(x+ornX[i]+0.24f, baseY+ornY[i]-0.02f);
                glVertex2f(x+ornX[i]+0.19f, baseY+ornY[i]-0.52f);
                glVertex2f(x+ornX[i]-0.19f, baseY+ornY[i]-0.52f);
            glEnd();
            glColor4ub(150, 82, 36, a);
            glLineWidth(1.0f);
            glBegin(GL_LINES);
                glVertex2f(x+ornX[i], baseY+ornY[i]+0.10f);
                glVertex2f(x+ornX[i], baseY+ornY[i]-0.02f);
            glEnd();
        }
    }
    glLineWidth(1.0f);
}

void DrawGiftBox4(float x, float y, unsigned char r, unsigned char g, unsigned char b) {
    glColor3ub(r, g, b);
    glBegin(GL_QUADS);
        glVertex2f(x-0.4f, y);      glVertex2f(x+0.4f, y);
        glVertex2f(x+0.4f, y+0.7f); glVertex2f(x-0.4f, y+0.7f);
    glEnd();
    glColor3ub(255, 255, 255);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        glVertex2f(x, y);           glVertex2f(x, y+0.7f);
        glVertex2f(x-0.4f, y+0.35f); glVertex2f(x+0.4f, y+0.35f);
    glEnd();
}

void DrawGiftBoxes4() {
    DrawGiftBox4(-2.0f, -9.0f, 200, 60, 70);
    DrawGiftBox4(-1.0f, -9.0f, 60, 140, 200);
    DrawGiftBox4( 2.2f, -9.0f, 220, 180, 60);
}

// ---- Snowman family --------------------------------------------------
void DrawSnowman4(float x, float scale) {
    float y = -9.0f;
    FilledCircle4(x, y+0.5f*scale, 0.7f*scale,  250, 250, 255, 255);
    FilledCircle4(x, y+1.5f*scale, 0.5f*scale,  250, 250, 255, 255);
    FilledCircle4(x, y+2.2f*scale, 0.35f*scale, 250, 250, 255, 255);
    glColor3ub(230, 120, 40);
    glBegin(GL_TRIANGLES);
        glVertex2f(x, y+2.2f*scale); glVertex2f(x+0.25f*scale, y+2.18f*scale); glVertex2f(x, y+2.15f*scale);
    glEnd();
    glColor3ub(40, 40, 45);
    FilledCircle4(x-0.1f*scale, y+2.25f*scale, 0.04f*scale, 30, 30, 30, 255);
    FilledCircle4(x+0.1f*scale, y+2.25f*scale, 0.04f*scale, 30, 30, 30, 255);
    FilledCircle4(x, y+1.6f*scale, 0.05f*scale, 40, 40, 45, 255);
    FilledCircle4(x, y+1.4f*scale, 0.05f*scale, 40, 40, 45, 255);
    glColor3ub(200, 50, 50);
    glBegin(GL_QUADS);
        glVertex2f(x-0.35f*scale, y+1.85f*scale); glVertex2f(x+0.35f*scale, y+1.85f*scale);
        glVertex2f(x+0.35f*scale, y+2.0f*scale);  glVertex2f(x-0.35f*scale, y+2.0f*scale);
    glEnd();
    glColor3ub(90, 60, 40);
    glLineWidth(1.5f);
    glBegin(GL_LINES);
        glVertex2f(x-0.5f*scale, y+1.55f*scale); glVertex2f(x-1.1f*scale, y+1.9f*scale);
        glVertex2f(x+0.5f*scale, y+1.55f*scale); glVertex2f(x+1.1f*scale, y+1.9f*scale);
    glEnd();
}

void DrawSnowmanFamily4() {
    if (SeasonWinter4() < 0.02f) return;   // replaced by the planters
    DrawSnowman4(20.0f, 1.0f);
    DrawSnowman4(21.6f, 0.6f);
    DrawSnowman4(18.6f, 0.5f);
}

// ---- Santa, already delivered, standing out on the plaza ------------------
//  Two things were wrong with the old figure.
//
//  Size: he was 2.5 units tall with a 0.32 head, which made him SMALLER than
//  the pedestrians walking in front of him -- backwards, since he stands
//  further back and everything further back should be smaller, not larger.
//  He is now built at roughly 1.35x with proportions that read at this scale:
//  a barrel torso, a real beard, boots and a proper sack.
//
//  Position: at x = 4.5 on y = -9 his hat reached y = -6.5, which is inside
//  the GIFTS shopfront band (x 2.3 .. 16.1, y -7.4 .. -1.0), so he appeared to
//  be standing INSIDE the shop window. Moving him forward to y = -10.2 puts
//  him on the plaza in front of the row, where he belongs.
//  Only the part of a figure ABOVE y = -7.4 can overlap a shopfront at all,
//  and at this size that is just the tip of his hat -- so he is parked in the
//  clear gap between the BAKERY front (ends x = -0.6) and the GIFTS front
//  (starts x = 2.3). Nothing of him crosses a window.
constexpr float SANTA_X4 = 0.85f;
constexpr float SANTA_Y4 = -10.2f;

void DrawSanta4() {
    if (SeasonWinter4() < 0.02f) return;   // replaced by the chestnut roaster
    const float x = SANTA_X4, y = SANTA_Y4;
    // 1.05 rather than the 1.35 first tried: he has to stand a little taller
    // than the crowd without out-scaling the walkers who are NEARER than he
    // is, at y = -13.9.
    const float S = 1.05f;

    DrawGroundShadow(x, y, 1.05f * S, 0.30f, 86);

    // Boots
    glColor3ub(26, 26, 30);
    for (int i = -1; i <= 1; i += 2) {
        glBegin(GL_QUADS);
            glVertex2f(x + i*0.10f*S - 0.24f*S, y);
            glVertex2f(x + i*0.10f*S + 0.24f*S, y);
            glVertex2f(x + i*0.10f*S + 0.24f*S, y + 0.42f*S);
            glVertex2f(x + i*0.10f*S - 0.24f*S, y + 0.42f*S);
        glEnd();
    }

    // Coat: barrel-shaped, widest at the belly
    glColor3ub(196, 44, 44);
    glBegin(GL_POLYGON);
        glVertex2f(x - 0.52f*S, y + 0.30f*S);
        glVertex2f(x + 0.52f*S, y + 0.30f*S);
        glVertex2f(x + 0.60f*S, y + 0.95f*S);
        glVertex2f(x + 0.46f*S, y + 1.62f*S);
        glVertex2f(x - 0.46f*S, y + 1.62f*S);
        glVertex2f(x - 0.60f*S, y + 0.95f*S);
    glEnd();
    // Shading down the left side, so he is not a flat red slab
    glColor4ub(150, 30, 32, 170);
    glBegin(GL_POLYGON);
        glVertex2f(x - 0.52f*S, y + 0.30f*S);
        glVertex2f(x - 0.22f*S, y + 0.30f*S);
        glVertex2f(x - 0.20f*S, y + 1.62f*S);
        glVertex2f(x - 0.46f*S, y + 1.62f*S);
        glVertex2f(x - 0.60f*S, y + 0.95f*S);
    glEnd();

    // Fur hem and cuffs
    glColor3ub(248, 248, 250);
    glBegin(GL_QUADS);
        glVertex2f(x - 0.56f*S, y + 0.28f*S); glVertex2f(x + 0.56f*S, y + 0.28f*S);
        glVertex2f(x + 0.56f*S, y + 0.50f*S); glVertex2f(x - 0.56f*S, y + 0.50f*S);
    glEnd();

    // Belt and buckle
    glColor3ub(30, 28, 32);
    glBegin(GL_QUADS);
        glVertex2f(x - 0.60f*S, y + 0.86f*S); glVertex2f(x + 0.60f*S, y + 0.86f*S);
        glVertex2f(x + 0.60f*S, y + 1.10f*S); glVertex2f(x - 0.60f*S, y + 1.10f*S);
    glEnd();
    glColor3ub(226, 188, 84);
    glBegin(GL_QUADS);
        glVertex2f(x - 0.17f*S, y + 0.88f*S); glVertex2f(x + 0.17f*S, y + 0.88f*S);
        glVertex2f(x + 0.17f*S, y + 1.08f*S); glVertex2f(x - 0.17f*S, y + 1.08f*S);
    glEnd();

    // Arms: one down at his side, one raised in a wave
    glColor3ub(196, 44, 44);
    glLineWidth(5.4f * S);
    float wave = sinf(twinklePhase4 * 1.6f) * 0.16f;
    glBegin(GL_LINES);
        glVertex2f(x - 0.48f*S, y + 1.46f*S); glVertex2f(x - 0.88f*S, y + 0.86f*S);
        glVertex2f(x + 0.48f*S, y + 1.46f*S);
        glVertex2f(x + 1.00f*S, y + (2.06f + wave)*S);
    glEnd();
    glColor3ub(248, 248, 250);                       // mittens
    FilledCircle4(x - 0.92f*S, y + 0.80f*S, 0.17f*S, 248, 248, 250, 255);
    FilledCircle4(x + 1.04f*S, y + (2.12f + wave)*S, 0.17f*S, 248, 248, 250, 255);

    // Head, beard, moustache
    FilledCircle4(x, y + 1.92f*S, 0.34f*S, 244, 202, 166, 255);
    glColor3ub(250, 250, 252);
    glBegin(GL_POLYGON);                             // beard, tapering to a point
        glVertex2f(x - 0.36f*S, y + 1.96f*S);
        glVertex2f(x + 0.36f*S, y + 1.96f*S);
        glVertex2f(x + 0.24f*S, y + 1.58f*S);
        glVertex2f(x,           y + 1.40f*S);
        glVertex2f(x - 0.24f*S, y + 1.58f*S);
    glEnd();
    FilledCircle4(x - 0.15f*S, y + 1.99f*S, 0.11f*S, 250, 250, 252, 255);   // moustache
    FilledCircle4(x + 0.15f*S, y + 1.99f*S, 0.11f*S, 250, 250, 252, 255);
    FilledCircle4(x,           y + 2.02f*S, 0.09f*S, 232, 158, 132, 255);   // nose
    glColor3ub(40, 34, 34);
    FilledCircle4(x - 0.13f*S, y + 2.12f*S, 0.045f*S, 40, 34, 34, 255);
    FilledCircle4(x + 0.13f*S, y + 2.12f*S, 0.045f*S, 40, 34, 34, 255);

    // Hat: a slumped cone, not a rigid triangle
    glColor3ub(196, 44, 44);
    glBegin(GL_POLYGON);
        glVertex2f(x - 0.36f*S, y + 2.20f*S);
        glVertex2f(x + 0.36f*S, y + 2.20f*S);
        glVertex2f(x + 0.52f*S, y + 2.72f*S);
        glVertex2f(x + 0.46f*S, y + 2.94f*S);
    glEnd();
    glColor3ub(248, 248, 250);
    glBegin(GL_QUADS);                               // hat band
        glVertex2f(x - 0.40f*S, y + 2.16f*S); glVertex2f(x + 0.40f*S, y + 2.16f*S);
        glVertex2f(x + 0.40f*S, y + 2.34f*S); glVertex2f(x - 0.40f*S, y + 2.34f*S);
    glEnd();
    FilledCircle4(x + 0.50f*S, y + 2.98f*S, 0.16f*S, 248, 248, 250, 255);   // bobble

    // Sack, leaning against his leg with a couple of parcels showing
    glColor3ub(146, 104, 62);
    glBegin(GL_POLYGON);
        glVertex2f(x - 1.62f*S, y + 0.02f*S);
        glVertex2f(x - 0.72f*S, y + 0.02f*S);
        glVertex2f(x - 0.66f*S, y + 0.92f*S);
        glVertex2f(x - 1.10f*S, y + 1.24f*S);
        glVertex2f(x - 1.70f*S, y + 0.86f*S);
    glEnd();
    glColor3ub(110, 76, 44);
    glLineWidth(1.6f * S);
    glBegin(GL_LINES);                               // the tie at the neck
        glVertex2f(x - 1.44f*S, y + 1.00f*S); glVertex2f(x - 0.84f*S, y + 0.90f*S);
    glEnd();
    glColor3ub(214, 78, 88);
    glBegin(GL_QUADS);
        glVertex2f(x - 1.36f*S, y + 1.06f*S); glVertex2f(x - 1.02f*S, y + 1.06f*S);
        glVertex2f(x - 1.02f*S, y + 1.36f*S); glVertex2f(x - 1.36f*S, y + 1.36f*S);
    glEnd();
    glColor3ub(226, 188, 84);
    glBegin(GL_LINES);
        glVertex2f(x - 1.19f*S, y + 1.06f*S); glVertex2f(x - 1.19f*S, y + 1.36f*S);
    glEnd();
    glLineWidth(1.0f);
}

// ---- Ice sculptures near the rink -------------------------------------
void DrawIceSculpture4(float x, float scale) {
    float y = -9.0f;
    glColor4ub(150, 210, 235, 220);
    glBegin(GL_TRIANGLES);
        glVertex2f(x-0.6f*scale, y); glVertex2f(x+0.6f*scale, y); glVertex2f(x, y+2.0f*scale);
    glEnd();
    glColor3ub(90, 160, 200);
    glLineWidth(1.5f);
    glBegin(GL_LINE_LOOP);
        glVertex2f(x-0.6f*scale, y); glVertex2f(x+0.6f*scale, y); glVertex2f(x, y+2.0f*scale);
    glEnd();
    glColor4ub(210, 235, 250, 180);
    glBegin(GL_TRIANGLES);
        glVertex2f(x-0.3f*scale, y+0.3f); glVertex2f(x+0.1f*scale, y+0.3f); glVertex2f(x-0.1f*scale, y+1.3f*scale);
    glEnd();
    FilledCircle4(x+0.15f*scale, y+1.3f*scale, 0.08f*scale, 255, 255, 255, 240);
}

void DrawIceSculptures4() {
    if (SeasonWinter4() < 0.02f) return;   // ice sculptures do not survive autumn
    DrawIceSculpture4(13.0f, 1.0f);
    DrawIceSculpture4(17.5f, 0.8f);
}

// Variant that places an ice sculpture on an arbitrary base Y (useful for
// putting sculptures on the terrace/footpath rather than the rink area).
void DrawIceSculptureAt(float x, float baseY, float scale) {
    float y = baseY;
    glColor4ub(150, 210, 235, 220);
    glBegin(GL_TRIANGLES);
        glVertex2f(x-0.6f*scale, y); glVertex2f(x+0.6f*scale, y); glVertex2f(x, y+2.0f*scale);
    glEnd();
    glColor3ub(90, 160, 200);
    glLineWidth(1.5f);
    glBegin(GL_LINE_LOOP);
        glVertex2f(x-0.6f*scale, y); glVertex2f(x+0.6f*scale, y); glVertex2f(x, y+2.0f*scale);
    glEnd();
    glColor4ub(210, 235, 250, 180);
    glBegin(GL_TRIANGLES);
        glVertex2f(x-0.3f*scale, y+0.3f); glVertex2f(x+0.1f*scale, y+0.3f); glVertex2f(x-0.1f*scale, y+1.3f*scale);
    glEnd();
    FilledCircle4(x+0.15f*scale, y+1.3f*scale, 0.08f*scale, 255, 255, 255, 240);
}

// ---- Small stage with a live band -------------------------------------
void DrawStage4() {
    float x = -56.5f, y = -9.0f;
    glColor3ub(110, 80, 55);
    glBegin(GL_QUADS);
        glVertex2f(x-3.0f, y-0.5f); glVertex2f(x+3.0f, y-0.5f);
        glVertex2f(x+3.0f, y);      glVertex2f(x-3.0f, y);
    glEnd();
    unsigned char cols[3][3] = { {140, 50, 60}, {50, 90, 140}, {60, 120, 70} };
    for (int i = 0; i < 3; i++) {
        float px = x - 1.6f + i * 1.6f;
        FilledCircle4(px, y+1.1f, 0.26f, 225, 185, 145, 255);
        glColor3ub(cols[i][0], cols[i][1], cols[i][2]);
        glBegin(GL_QUADS);
            glVertex2f(px-0.22f, y+0.1f); glVertex2f(px+0.22f, y+0.1f);
            glVertex2f(px+0.2f, y+0.85f); glVertex2f(px-0.2f, y+0.85f);
        glEnd();
    }
    glColor3ub(120, 80, 40);
    FilledCircle4(x, y+0.5f, 0.25f, 120, 80, 40, 255);
    glColor3ub(40, 40, 45);
    glLineWidth(1.5f);
    glBegin(GL_LINES); glVertex2f(x-1.6f, y-0.1f); glVertex2f(x-1.6f, y+0.9f); glEnd();
    FilledCircle4(x-1.6f, y+0.95f, 0.1f, 60, 60, 65, 255);
}

// ---- Nutcracker soldier statue -----------------------------------------
// Moved forward from y = -9 to y = -10.4: at the old height his shako reached
// y = -6.1, inside the BAKERY shopfront band (x -13.8 .. -0.6, y -7.4 .. -1.0),
// so the figure stood inside the shop window instead of in front of it.
void DrawNutcracker4(float x) {
    float y = -10.4f;
    glColor3ub(20, 20, 25);
    glBegin(GL_QUADS); glVertex2f(x-0.4f, y); glVertex2f(x+0.4f, y); glVertex2f(x+0.4f, y+0.3f); glVertex2f(x-0.4f, y+0.3f); glEnd();
    glColor3ub(240, 240, 235);
    glBegin(GL_QUADS); glVertex2f(x-0.35f, y+0.3f); glVertex2f(x+0.35f, y+0.3f); glVertex2f(x+0.35f, y+0.9f); glVertex2f(x-0.35f, y+0.9f); glEnd();
    glColor3ub(180, 30, 40);
    glBegin(GL_QUADS); glVertex2f(x-0.4f, y+0.9f); glVertex2f(x+0.4f, y+0.9f); glVertex2f(x+0.38f, y+1.9f); glVertex2f(x-0.38f, y+1.9f); glEnd();
    glColor3ub(230, 200, 80);
    for (int i = 0; i < 3; i++) FilledCircle4(x, y+1.1f+i*0.25f, 0.05f, 230, 200, 80, 255);
    glColor3ub(30, 40, 90);
    glLineWidth(3.0f);
    glBegin(GL_LINES);
        glVertex2f(x-0.4f, y+1.7f); glVertex2f(x-0.65f, y+1.2f);
        glVertex2f(x+0.4f, y+1.7f); glVertex2f(x+0.65f, y+1.2f);
    glEnd();
    FilledCircle4(x, y+2.2f, 0.32f, 240, 200, 170, 255);
    glColor3ub(20, 20, 25);
    glBegin(GL_QUADS); glVertex2f(x-0.32f, y+2.4f); glVertex2f(x+0.32f, y+2.4f); glVertex2f(x+0.32f, y+2.9f); glVertex2f(x-0.32f, y+2.9f); glEnd();
    glColor3ub(240, 240, 235);
    glBegin(GL_QUADS); glVertex2f(x-0.35f, y+2.35f); glVertex2f(x+0.35f, y+2.35f); glVertex2f(x+0.35f, y+2.5f); glVertex2f(x-0.35f, y+2.5f); glEnd();
    glColor3ub(30, 30, 30);
    glBegin(GL_QUADS); glVertex2f(x-0.25f, y+2.1f); glVertex2f(x+0.25f, y+2.1f); glVertex2f(x+0.25f, y+2.18f); glVertex2f(x-0.25f, y+2.18f); glEnd();
}

// ---- Clock tower with a swinging pendulum ---------------------------------
void DrawClockTower4() {
    // topY has to clear the shop roofs on either side (SKATES peaks at 16.6,
    // CIDER at 15.3) or the "tower" reads as the shortest thing on the row.
    float x = clockX4, baseY = -6.0f, topY = 21.0f;
    glColor3ub(60, 55, 65);
    glBegin(GL_QUADS);
        glVertex2f(x-1.5f, baseY); glVertex2f(x+1.5f, baseY);
        glVertex2f(x+1.5f, topY);  glVertex2f(x-1.5f, topY);
    glEnd();
    // Backlit dial: it is a night scene, so the face has to be a light source.
    FilledCircle4(x, topY-2.0f, 2.8f, 255, 226, 170, 34);
    FilledCircle4(x, topY-2.0f, 1.3f, 250, 242, 214, 255);
    // Tick marks around the dial
    glColor3ub(70, 70, 78);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        for (int t = 0; t < 12; t++) {
            float a = PI4 * 0.5f - t * (2.0f * PI4 / 12.0f);
            float inner = (t % 3 == 0) ? 0.92f : 1.05f;
            glVertex2f(x + 1.22f*cosf(a), (topY-2.0f) + 1.22f*sinf(a));
            glVertex2f(x + inner*cosf(a), (topY-2.0f) + inner*sinf(a));
        }
    glEnd();

    // The hands show the real wall-clock time, so a viewer who glances at
    // the tower gets something true rather than an arbitrary sweep -- and
    // the second hand gives the scene a visible, steady tick.
    time_t raw = time(nullptr);
    struct tm* lt = localtime(&raw);
    float secs  = lt ? (float)lt->tm_sec  : 0.0f;
    float mins  = lt ? (float)lt->tm_min + secs / 60.0f      : 0.0f;
    float hours = lt ? (float)(lt->tm_hour % 12) + mins / 60.0f : 0.0f;

    float secA  = PI4 * 0.5f - secs  * (2.0f * PI4 / 60.0f);
    float minA  = PI4 * 0.5f - mins  * (2.0f * PI4 / 60.0f);
    float hourA = PI4 * 0.5f - hours * (2.0f * PI4 / 12.0f);

    glColor3ub(40, 40, 45);
    glLineWidth(2.6f);
    glBegin(GL_LINES);
        glVertex2f(x, topY-2.0f); glVertex2f(x + 0.60f*cosf(hourA), (topY-2.0f) + 0.60f*sinf(hourA));
    glEnd();
    glLineWidth(1.8f);
    glBegin(GL_LINES);
        glVertex2f(x, topY-2.0f); glVertex2f(x + 0.95f*cosf(minA), (topY-2.0f) + 0.95f*sinf(minA));
    glEnd();
    glColor3ub(170, 50, 55);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        glVertex2f(x - 0.18f*cosf(secA), (topY-2.0f) - 0.18f*sinf(secA));
        glVertex2f(x + 1.05f*cosf(secA), (topY-2.0f) + 1.05f*sinf(secA));
    glEnd();
    FilledCircle4(x, topY-2.0f, 0.09f, 40, 40, 45, 255);
    glColor3ub(150, 40, 50);
    glBegin(GL_TRIANGLES);
        glVertex2f(x-2.0f, topY); glVertex2f(x+2.0f, topY); glVertex2f(x, topY+2.5f);
    glEnd();
    // Pendulum, glimpsed through a slit below the clock face
    glColor3ub(20, 18, 22);
    glBegin(GL_QUADS);
        glVertex2f(x-0.5f, baseY+1.0f); glVertex2f(x+0.5f, baseY+1.0f);
        glVertex2f(x+0.5f, topY-3.5f);  glVertex2f(x-0.5f, topY-3.5f);
    glEnd();
    float swing = sinf(pendulumAngle4) * 0.35f;
    float pivotX = x, pivotY = topY - 3.5f;
    float bobX = pivotX + swing * 3.0f;
    float bobY = pivotY - 2.5f;
    glColor3ub(180, 170, 120);
    glLineWidth(1.5f);
    glBegin(GL_LINES); glVertex2f(pivotX, pivotY); glVertex2f(bobX, bobY); glEnd();
    FilledCircle4(bobX, bobY, 0.3f, 200, 180, 90, 255);
}

void UpdatePendulum4(int) {
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdatePendulum4, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating4) pendulumAngle4 += 0.05f;
    glutTimerFunc(20, UpdatePendulum4, 0);
}

// ---- Small caroling group --------------------------------------------
void DrawCarolers4() {
    float baseX = 45.5f, y = -9.5f;
    unsigned char coatCols[3][3] = { {150, 40, 60}, {40, 90, 140}, {90, 140, 60} };
    for (int i = 0; i < 3; i++) {
        float x = baseX + (i-1) * 0.9f;
        FilledCircle4(x, y+1.4f, 0.28f, 225, 185, 145, 255);
        glColor3ub(coatCols[i][0], coatCols[i][1], coatCols[i][2]);
        glBegin(GL_QUADS);
            glVertex2f(x-0.25f, y+0.3f); glVertex2f(x+0.25f, y+0.3f);
            glVertex2f(x+0.22f, y+1.15f); glVertex2f(x-0.22f, y+1.15f);
        glEnd();
    }
    // Music notes rising off the group. They used to climb to y = -4.5, which
    // is inside the SKATES shopfront band (x 34.5 .. 45.9, y -7.4 .. -1.0), so
    // they drifted up through the shop window. The rise is now clamped to stay
    // under the fronts, and they fade out before they get there anyway.
    const float noteCeil = -7.7f;
    for (int i = 0; i < 3; i++) {
        float t = fmodf(twinklePhase4*3.0f + i*0.7f, 2.0f);
        float nx = baseX - 0.5f + i*0.6f + sinf(t*3.0f)*0.3f;
        float ny = y + 1.9f + t*0.55f;
        if (ny > noteCeil) ny = noteCeil;
        unsigned char alpha = (unsigned char)(200 * (1.0f - t/2.0f));
        FilledCircle4(nx, ny, 0.12f, 255, 255, 255, alpha);
        // A stem, so they read as notes rather than snowflakes
        glColor4ub(255, 255, 255, alpha);
        glLineWidth(1.2f);
        glBegin(GL_LINES);
            glVertex2f(nx + 0.11f, ny); glVertex2f(nx + 0.11f, ny + 0.34f);
        glEnd();
        glLineWidth(1.0f);
    }
}

// ---- Occasional firework bursts --------------------------------------
void DrawFireworks4() {
    for (int i = 0; i < MAX_FIREWORKS4; i++) {
        const Firework4& f = fireworks4[i];
        if (f.state == FW_INACTIVE) continue;

        if (f.state == FW_RISING) {
            // Rocket: bright head plus a short fading exhaust tail.
            FilledCircle4(f.x, f.y, 0.18f, 255, 240, 200, 255);
            for (int t = 1; t <= 5; t++) {
                FilledCircle4(f.x, f.y - t * 0.5f, 0.13f - t * 0.02f,
                              255, 190, 110, (unsigned char)(150 - t * 28));
            }
            continue;
        }

        // Bursting: each spark flies along its own stored direction and
        // speed, with gravity pulling the whole shell down over time. The
        // old version spread sparks purely radially, which read as a comet.
        float t = f.phase;
        for (int s = 0; s < 18; s++) {
            float d  = f.sparkSpd[s] * t * 7.0f;
            float px = f.x + d * cosf(f.sparkAng[s]);
            float py = f.y + d * sinf(f.sparkAng[s]) - t * t * 5.0f;
            unsigned char a = (unsigned char)(255 * (1.0f - t));
            FilledCircle4(px, py, 0.16f * (1.0f - t * 0.5f), f.r, f.g, f.b, a);
        }
        // Flash halo at the burst centre early on
        if (t < 0.25f) {
            FilledCircle4(f.x, f.y, 2.2f * t / 0.25f, f.r, f.g, f.b,
                          (unsigned char)(90 * (1.0f - t / 0.25f)));
        }
    }
}

// Bursting fireworks briefly light the snow below them.
void DrawFireworkGroundFlash4() {
    float glow = 0.0f;
    unsigned char gr = 255, gg = 255, gb = 255;
    for (int i = 0; i < MAX_FIREWORKS4; i++) {
        if (fireworks4[i].state != FW_BURSTING) continue;
        float g = (1.0f - fireworks4[i].phase) * 0.6f;
        if (g > glow) { glow = g; gr = fireworks4[i].r; gg = fireworks4[i].g; gb = fireworks4[i].b; }
    }
    if (glow <= 0.01f) return;
    glColor4ub(gr, gg, gb, (unsigned char)(38 * glow));
    glBegin(GL_QUADS);
        glVertex2f(-60, -40); glVertex2f(60, -40);
        glVertex2f(60, -6);   glVertex2f(-60, -6);
    glEnd();
}

void UpdateFireworks4(int) {
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateFireworks4, 0); return; }
    if (isAnimating4) {
        fireworkCooldown4--;
        if (fireworkCooldown4 <= 0) {
            for (int i = 0; i < MAX_FIREWORKS4; i++) {
                if (fireworks4[i].state == FW_INACTIVE) {
                    Firework4& f = fireworks4[i];
                    f.state   = FW_RISING;
                    f.x       = -45.0f + (rand() % 900) / 10.0f;
                    f.y       = -5.0f;                       // launches from the plaza
                    f.targetY = 20.0f + (rand() % 120) / 10.0f;
                    f.phase   = 0.0f;
                    unsigned char cols[4][3] = { {230,60,60}, {60,200,230}, {230,210,60}, {210,110,235} };
                    int c = rand() % 4;
                    f.r = cols[c][0]; f.g = cols[c][1]; f.b = cols[c][2];
                    for (int s = 0; s < 18; s++) {
                        f.sparkAng[s] = (float)s / 18.0f * 2.0f * PI4
                                      + (rand() % 40 - 20) / 300.0f;
                        f.sparkSpd[s] = 0.75f + (rand() % 50) / 100.0f;
                    }
                    break;
                }
            }
            fireworkCooldown4 = 180 + rand() % 260;
        }
        for (int i = 0; i < MAX_FIREWORKS4; i++) {
            Firework4& f = fireworks4[i];
            if (f.state == FW_RISING) {
                f.y += 0.55f;
                if (f.y >= f.targetY) { f.state = FW_BURSTING; f.phase = 0.0f; }
            } else if (f.state == FW_BURSTING) {
                f.phase += 0.018f;
                if (f.phase >= 1.0f) f.state = FW_INACTIVE;
            }
        }
    }
    glutTimerFunc(30, UpdateFireworks4, 0);
}

// ---- Aurora / northern lights -- a rare, slow-fading sky effect -----------
//  Unchanged in behaviour: X still forces it, the timer still runs it on its
//  own cooldown. What changed is that it is now part of the sky rather than
//  three flat ribbons laid over it. Each curtain is drawn as a bright core
//  with two wider, fainter shells either side of it, so the edges bleed into
//  the air instead of stopping on a hard line, and the whole effect is scaled
//  by NightT4() so a daylight sky does not end up with ribbons stapled to it.
void DrawAurora4() {
    if (!auroraActive4) return;
    float envelope = sinf(auroraTimer4 * PI4);   // fades in, peaks, fades out
    if (envelope <= 0.0f) return;
    envelope *= 0.18f + 0.82f * NightT4();

    const unsigned char cols[4][3] = {
        { 56, 214, 142 }, { 74, 202, 188 }, { 82, 142, 230 }, { 168, 104, 220 }
    };
    for (int band = 0; band < 4; band++) {
        float baseY = 28.5f + band * 2.7f;
        float h     = 5.4f + band * 1.2f;
        for (int shell = 0; shell < 3; shell++) {
            float spread = 1.0f + shell * 1.05f;                 // wider, fainter
            float aMul   = (shell == 0) ? 1.0f : 0.34f / (float)shell;
            glBegin(GL_QUAD_STRIP);
            for (int i = 0; i <= 30; i++) {
                float x    = -62.0f + i * 4.2f;
                float wave = sinf(auroraPhase4 + i * 0.30f + band * 1.45f) * 2.7f
                           + sinf(auroraPhase4 * 0.55f + i * 0.12f) * 1.5f;
                float yc   = baseY + wave;
                // Vertical rays: the brightness ripples along the curtain,
                // which is what an aurora does and a flat ribbon does not.
                float ray  = 0.45f + 0.55f * (0.5f + 0.5f * sinf(auroraPhase4 * 1.6f + i * 0.85f + band));
                unsigned char a = (unsigned char)(66.0f * envelope * aMul * ray);
                glColor4ub(cols[band][0], cols[band][1], cols[band][2], a);
                glVertex2f(x, yc + h * spread * 0.55f);
                glColor4ub(cols[band][0], cols[band][1], cols[band][2], (unsigned char)(a * 0.22f));
                glVertex2f(x, yc - h * spread * 0.65f);
            }
            glEnd();
        }
    }
    // A very faint green cast on the air below the curtains, so they light
    // the sky they hang in rather than floating clear of it.
    glBegin(GL_QUADS);
        glColor4ub(70, 190, 150, (unsigned char)(26.0f * envelope));
        glVertex2f(-60.0f, 30.0f); glVertex2f(60.0f, 30.0f);
        glColor4ub(70, 190, 150, 0);
        glVertex2f( 60.0f,  8.0f); glVertex2f(-60.0f, 8.0f);
    glEnd();
}

void UpdateAurora4(int) {
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateAurora4, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating4) {
        auroraPhase4 += 0.02f;
        if (auroraActive4) {
            auroraTimer4 += 0.006f;
            if (auroraTimer4 >= 1.0f) { auroraActive4 = false; auroraCooldown4 = 500 + rand() % 600; }
        } else {
            auroraCooldown4--;
            if (auroraCooldown4 <= 0) { auroraActive4 = true; auroraTimer4 = 0.0f; }
        }
    }
    glutTimerFunc(30, UpdateAurora4, 0);
}

void SpawnStallSteam4() {
    for (int i = 0; i < MAX_STEAM4; i++) {
        if (!stallSteam4[i].active) {
            stallSteam4[i].active = true;
            stallSteam4[i].x = stalls4[0].x + (rand()%40-20)/100.0f;  // the cocoa urn
            stallSteam4[i].y = StallCounterTopY4() + 0.4f;
            stallSteam4[i].vy = 0.04f + (rand()%20)/1000.0f;
            stallSteam4[i].alpha = 130.0f;
            stallSteam4[i].size = 0.35f + (rand()%15)/100.0f;
            return;
        }
    }
}

void DrawStallSteam4() {
    for (int i = 0; i < MAX_STEAM4; i++) {
        if (!stallSteam4[i].active) continue;
        FilledCircle4(stallSteam4[i].x, stallSteam4[i].y, stallSteam4[i].size, 220, 220, 225, (unsigned char)stallSteam4[i].alpha);
    }
}

void UpdateStallSteam4(int) {
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateStallSteam4, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating4) {
        static int acc = 0;
        acc++;
        if (acc > 8) { SpawnStallSteam4(); acc = 0; }
        for (int i = 0; i < MAX_STEAM4; i++) {
            if (!stallSteam4[i].active) continue;
            stallSteam4[i].y += stallSteam4[i].vy;
            stallSteam4[i].size += 0.008f;
            stallSteam4[i].alpha -= 1.4f;
            if (stallSteam4[i].alpha <= 0) stallSteam4[i].active = false;
        }
    }
    glutTimerFunc(35, UpdateStallSteam4, 0);
}

void SpawnChimneySmoke4() {
    for (int i = 0; i < MAX_SMOKE4; i++) {
        if (!chimneySmoke4[i].active) {
            chimneySmoke4[i].active = true;
            chimneySmoke4[i].x = ChimneyX4() + (rand()%30-15)/100.0f;
            chimneySmoke4[i].y = ShopRoofY4(shops4[1], ChimneyX4()) + 3.4f;
            chimneySmoke4[i].vy = 0.03f + (rand()%15)/1000.0f;
            chimneySmoke4[i].alpha = 110.0f;
            chimneySmoke4[i].size = 0.4f + (rand()%15)/100.0f;
            return;
        }
    }
}

void DrawChimneySmoke4() {
    for (int i = 0; i < MAX_SMOKE4; i++) {
        if (!chimneySmoke4[i].active) continue;
        FilledCircle4(chimneySmoke4[i].x, chimneySmoke4[i].y, chimneySmoke4[i].size, 180, 180, 190, (unsigned char)chimneySmoke4[i].alpha);
    }
}

void UpdateChimneySmoke4(int) {
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateChimneySmoke4, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating4) {
        static int acc = 0;
        acc++;
        if (acc > 10) { SpawnChimneySmoke4(); acc = 0; }
        for (int i = 0; i < MAX_SMOKE4; i++) {
            if (!chimneySmoke4[i].active) continue;
            chimneySmoke4[i].y += chimneySmoke4[i].vy;
            chimneySmoke4[i].x += 0.008f;
            chimneySmoke4[i].size += 0.006f;
            chimneySmoke4[i].alpha -= 1.0f;
            if (chimneySmoke4[i].alpha <= 0) chimneySmoke4[i].active = false;
        }
    }
    glutTimerFunc(35, UpdateChimneySmoke4, 0);
}

// ---- Settled snow: caps on roofs, tree tiers, stalls, clock spire -------
void DrawSnowAccumulation4() {
    // Every cap, ledge and drift in here is settled SNOW, so the whole pass
    // fades out as autumn comes in rather than each piece being branched.
    if (SeasonWinter4() < 0.02f) return;
    float c = SnowDepth4();                 // 0.20 .. 0.90
    glColor3ub(246, 249, 255);

    // Shop roof caps, thickness scaling with cover
    for (int i = 0; i < NUM_SHOPS4; i++) {
        const Shop4& sh = shops4[i];
        float topY = -6.0f + sh.h;
        float th   = 0.35f + 1.05f * c;
        glBegin(GL_QUADS);
            glVertex2f(sh.x - sh.w*0.5f - 0.5f, topY);
            glVertex2f(sh.x + sh.w*0.5f + 0.5f, topY);
            glVertex2f(sh.x + sh.w*0.5f + 0.2f, topY + th);
            glVertex2f(sh.x - sh.w*0.5f - 0.2f, topY + th);
        glEnd();
        // Icicles hanging off the eaves once there is real snow
        if (c > 0.4f) {
            int n = 3 + (int)(c * 4);
            for (int k = 0; k < n; k++) {
                float ix = sh.x - sh.w*0.42f + k * (sh.w * 0.84f / (n - 1));
                float len = 0.30f + 0.55f * c * (0.6f + 0.4f * sinf(ix));
                glBegin(GL_TRIANGLES);
                    glVertex2f(ix - 0.10f, topY);
                    glVertex2f(ix + 0.10f, topY);
                    glVertex2f(ix,         topY - len);
                glEnd();
            }
        }
    }

    // Snow lying along the two slopes of each stall canopy. It used to be one
    // flat bar drawn at the apex height across the full canopy width, which
    // floated clear of the tent below it.
    for (int i = 0; i < NUM_STALLS4; i++) {
        float sx   = stalls4[i].x;
        float half = StallHalfW4(), eave = StallCounterTopY4(), apex = StallApexY4();
        float th   = 0.30f + 0.6f * c;
        glBegin(GL_QUADS);
            glVertex2f(sx-half, eave);      glVertex2f(sx, apex);
            glVertex2f(sx,      apex+th);   glVertex2f(sx-half, eave+th);
            glVertex2f(sx+half, eave);      glVertex2f(sx, apex);
            glVertex2f(sx,      apex+th);   glVertex2f(sx+half, eave+th);
        glEnd();
    }

    // Snow banks along the left and right edges of the plaza
    glBegin(GL_QUADS);
        glVertex2f(-60.0f, -6.0f);
        glVertex2f(-46.0f, -6.0f);
        glVertex2f(-48.0f, -6.0f + 1.2f * c);
        glVertex2f(-60.0f, -6.0f + 1.9f * c);
    glEnd();
    glBegin(GL_QUADS);
        glVertex2f(60.0f, -6.0f);
        glVertex2f(46.0f, -6.0f);
        glVertex2f(48.0f, -6.0f + 1.2f * c);
        glVertex2f(60.0f, -6.0f + 1.9f * c);
    glEnd();

    // Clock tower spire cap
    glBegin(GL_TRIANGLES);
        glVertex2f(clockX4 - 1.5f * c, 16.4f);
        glVertex2f(clockX4 + 1.5f * c, 16.4f);
        glVertex2f(clockX4,            16.4f + 1.2f * c);
    glEnd();
}

// ---- Squall: layered drifting bands of snow-haze --------------------------
// ============================================================================
//  AUTUMN EQUIVALENTS
// ----------------------------------------------------------------------------
//  The four things in this plaza that only make sense in deep winter -- the
//  ice rink, the snowman family, the dressed Christmas tree and Santa -- each
//  get a counterpart that occupies the same ground. Hiding them instead would
//  have left four visible holes in the square; swapping them means both
//  seasons look deliberate.
//
//  Each pair crossfades on seasonT4, so for a second or two mid-transition the
//  rink really is dissolving into the fountain rather than one popping out.
// ============================================================================

// ---- Plaza fountain, where the rink is in winter --------------------------
void DrawFountainAutumn4() {
    if (SeasonAutumn4() < 0.02f) return;
    const unsigned char A = AutumnA4(255);
    const float cx = rinkCX4, cy = rinkCY4;

    // Wet apron where splash has darkened the paving
    glColor4ub(MixB4(74, 108), MixB4(80, 100), MixB4(72, 86), AutumnA4(120));
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(cx, cy);
        for (int i = 0; i <= 30; i++) {
            float a = (float)i / 30 * 2 * PI4;
            glVertex2f(cx + 7.4f*cosf(a), cy + 2.7f*sinf(a));
        }
    glEnd();

    // Stone basin rim
    glColor4ub(MixB4(112, 156), MixB4(110, 150), MixB4(104, 138), A);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(cx, cy);
        for (int i = 0; i <= 30; i++) {
            float a = (float)i / 30 * 2 * PI4;
            glVertex2f(cx + 6.0f*cosf(a), cy + 2.2f*sinf(a));
        }
    glEnd();
    // Water inside it
    glColor4ub(MixB4(30, 58), MixB4(62, 104), MixB4(74, 118), A);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(cx, cy);
        for (int i = 0; i <= 30; i++) {
            float a = (float)i / 30 * 2 * PI4;
            glVertex2f(cx + 5.1f*cosf(a), cy + 1.75f*sinf(a));
        }
    glEnd();
    // Coping stones round the rim
    glColor4ub(MixB4(86, 124), MixB4(84, 118), MixB4(80, 108), A);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        for (int i = 0; i < 20; i++) {
            float a = (float)i / 20 * 2 * PI4;
            glVertex2f(cx + 5.1f*cosf(a), cy + 1.75f*sinf(a));
            glVertex2f(cx + 6.0f*cosf(a), cy + 2.2f*sinf(a));
        }
    glEnd();

    // Central pedestal and bowl
    glColor4ub(MixB4(104, 148), MixB4(102, 142), MixB4(96, 130), A);
    glBegin(GL_QUADS);
        glVertex2f(cx - 0.7f, cy);        glVertex2f(cx + 0.7f, cy);
        glVertex2f(cx + 0.5f, cy + 2.1f); glVertex2f(cx - 0.5f, cy + 2.1f);
    glEnd();
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(cx, cy + 2.3f);
        for (int i = 0; i <= 16; i++) {
            float a = PI4 + PI4 * ((float)i / 16.0f);
            glVertex2f(cx + 1.7f*cosf(a), cy + 2.3f + 0.6f*sinf(a));
        }
    glEnd();

    // Jets arcing out of the bowl and falling back into the basin
    for (int j = 0; j < 7; j++) {
        float spread = -1.0f + 2.0f * (float)j / 6.0f;
        float vx = spread * 3.2f, vy = 3.3f - fabsf(spread) * 0.9f;
        glColor4ub(214, 236, 248, AutumnA4(180));
        glLineWidth(1.5f);
        glBegin(GL_LINE_STRIP);
            for (int k = 0; k <= 12; k++) {
                float t  = k / 12.0f * 0.55f;
                float px = cx + vx * t;
                float py = cy + 2.5f + vy * t - 0.5f * 22.0f * t * t;
                if (py < cy + 0.1f) break;
                glVertex2f(px, py);
            }
        glEnd();
        // A droplet running the arc, so the water reads as moving
        float dt = fmodf(firePhase4 * 0.5f + j * 0.14f, 0.55f);
        float dx = cx + vx * dt;
        float dy = cy + 2.5f + vy * dt - 0.5f * 22.0f * dt * dt;
        if (dy > cy + 0.1f) FilledCircle4(dx, dy, 0.13f, 236, 248, 255, AutumnA4(220));
    }

    // Expanding rings where the jets land
    for (int r = 0; r < 3; r++) {
        float t   = fmodf(firePhase4 * 0.30f + r * 0.33f, 1.0f);
        float rad = 1.0f + t * 3.6f;
        glColor4ub(228, 244, 252, AutumnA4((unsigned char)(130 * (1.0f - t))));
        glLineWidth(1.2f);
        glBegin(GL_LINE_LOOP);
            for (int i = 0; i < 20; i++) {
                float a = (float)i / 20 * 2 * PI4;
                glVertex2f(cx + rad*cosf(a), cy + rad*0.34f*sinf(a));
            }
        glEnd();
    }

    // Two people sitting on the rim with their backs to us
    for (int i = 0; i < 2; i++) {
        float sx = cx + (i == 0 ? -4.2f : 3.6f);
        float sy = cy + (i == 0 ? -1.2f : -1.5f);
        unsigned char cr = (i == 0) ? 132 : 62, cg = (i == 0) ? 58 : 78, cb = (i == 0) ? 60 : 116;
        glColor4ub(cr, cg, cb, A);
        glBegin(GL_QUADS);
            glVertex2f(sx - 0.42f, sy);        glVertex2f(sx + 0.42f, sy);
            glVertex2f(sx + 0.36f, sy + 1.24f); glVertex2f(sx - 0.36f, sy + 1.24f);
        glEnd();
        FilledCircle4(sx, sy + 1.56f, 0.30f, 226, 188, 152, A);
        FilledCircle4(sx, sy + 1.74f, 0.27f, cr, cg, cb, A);
    }
    glLineWidth(1.0f);
}

// ---- Planters, where the snowmen stand in winter --------------------------
void DrawPlanter4(float x, float scale) {
    const unsigned char A = AutumnA4(255);
    const float y = -9.0f;

    DrawGroundShadow(x, y, 1.5f * scale, 0.28f, AutumnA4(80));

    // Tapered terracotta tub
    glColor4ub(MixB4(120, 176), MixB4(62, 96), MixB4(44, 68), A);
    glBegin(GL_QUADS);
        glVertex2f(x - 1.05f*scale, y);
        glVertex2f(x + 1.05f*scale, y);
        glVertex2f(x + 1.28f*scale, y + 1.55f*scale);
        glVertex2f(x - 1.28f*scale, y + 1.55f*scale);
    glEnd();
    glColor4ub(MixB4(142, 198), MixB4(76, 112), MixB4(54, 80), A);
    glBegin(GL_QUADS);
        glVertex2f(x - 1.34f*scale, y + 1.48f*scale);
        glVertex2f(x + 1.34f*scale, y + 1.48f*scale);
        glVertex2f(x + 1.34f*scale, y + 1.80f*scale);
        glVertex2f(x - 1.34f*scale, y + 1.80f*scale);
    glEnd();

    // Autumn planting spilling over the rim
    const unsigned char blooms[4][3] = {
        {198, 76, 52}, {226, 150, 46}, {160, 54, 62}, {212, 186, 70}
    };
    for (int i = 0; i < 9; i++) {
        float h = sinf((x * 7.0f + i) * 12.9898f) * 43758.5453f;
        float j = h - floorf(h);
        float bx = x + (-1.05f + 0.26f * i) * scale;
        float by = y + (1.85f + 0.55f * j) * scale;
        glColor4ub(54, 92, 48, A);
        glLineWidth(1.4f);
        glBegin(GL_LINES);
            glVertex2f(bx, y + 1.70f*scale); glVertex2f(bx, by);
        glEnd();
        const unsigned char* c = blooms[i % 4];
        FilledCircle4(bx, by, 0.30f * scale, c[0], c[1], c[2], A);
    }
    glLineWidth(1.0f);
}

void DrawPlantersAutumn4() {
    if (SeasonAutumn4() < 0.02f) return;
    DrawPlanter4(20.0f, 1.0f);
    DrawPlanter4(21.9f, 0.7f);
    DrawPlanter4(18.4f, 0.6f);

    // A stack of pumpkins beside them -- the one prop that says autumn on
    // sight, the way a snowman says winter.
    const float px = 23.8f, py = -9.0f;
    const unsigned char A = AutumnA4(255);
    DrawGroundShadow(px, py, 1.3f, 0.26f, AutumnA4(80));
    for (int i = 0; i < 3; i++) {
        float r = 0.86f - i * 0.22f;
        float cy = py + r + i * 1.20f;
        glColor4ub(MixB4(154, 226), MixB4(78, 128), MixB4(30, 46), A);
        FilledCircle4(px, cy, r, MixB4(154, 226), MixB4(78, 128), MixB4(30, 46), A);
        glColor4ub(MixB4(120, 186), MixB4(58, 100), MixB4(22, 34), A);
        glLineWidth(1.2f);
        glBegin(GL_LINES);
            glVertex2f(px - r*0.45f, cy - r*0.55f); glVertex2f(px - r*0.45f, cy + r*0.55f);
            glVertex2f(px + r*0.45f, cy - r*0.55f); glVertex2f(px + r*0.45f, cy + r*0.55f);
        glEnd();
        glColor4ub(72, 96, 44, A);
        glLineWidth(2.0f);
        glBegin(GL_LINES);
            glVertex2f(px, cy + r*0.9f); glVertex2f(px + 0.12f, cy + r*1.35f);
        glEnd();
    }
    glLineWidth(1.0f);
}

// ---- Chestnut roaster, where Santa stands in winter -----------------------
void DrawChestnutRoaster4() {
    if (SeasonAutumn4() < 0.02f) return;
    const unsigned char A = AutumnA4(255);
    const float x = SANTA_X4, y = SANTA_Y4;

    DrawGroundShadow(x, y, 1.5f, 0.30f, AutumnA4(86));

    // Brazier on legs
    glColor4ub(44, 42, 46, A);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        glVertex2f(x - 0.72f, y); glVertex2f(x - 0.52f, y + 1.05f);
        glVertex2f(x + 0.72f, y); glVertex2f(x + 0.52f, y + 1.05f);
    glEnd();
    glColor4ub(58, 54, 58, A);
    glBegin(GL_QUADS);
        glVertex2f(x - 0.92f, y + 1.02f); glVertex2f(x + 0.92f, y + 1.02f);
        glVertex2f(x + 0.78f, y + 1.72f); glVertex2f(x - 0.78f, y + 1.72f);
    glEnd();

    // Coals and the heat coming off them
    float flick = 0.72f + 0.28f * sinf(firePhase4 * 3.0f);
    glColor4ub(236, 118, 40, AutumnA4((unsigned char)(210 * flick)));
    glBegin(GL_QUADS);
        glVertex2f(x - 0.80f, y + 1.62f); glVertex2f(x + 0.80f, y + 1.62f);
        glVertex2f(x + 0.80f, y + 1.80f); glVertex2f(x - 0.80f, y + 1.80f);
    glEnd();
    DrawSoftEllipse(x, y + 1.9f, 2.6f * flick, 1.5f * flick,
                    255, 150, 70, AutumnA4((unsigned char)(70 * flick * NightT4())), 3);
    for (int i = 0; i < 5; i++) {
        float t = fmodf(firePhase4 * 0.34f + i * 0.2f, 1.0f);
        FilledCircle4(x - 0.5f + i * 0.25f + sinf(t * 5.0f + i) * 0.25f,
                      y + 1.9f + t * 2.4f, 0.09f * (1.0f - t),
                      255, 186, 96, AutumnA4((unsigned char)(200 * (1.0f - t))));
    }

    // Chestnuts on the grill
    for (int i = 0; i < 6; i++)
        FilledCircle4(x - 0.62f + i * 0.25f, y + 1.84f, 0.13f,
                      MixB4(92, 128), MixB4(52, 74), MixB4(34, 44), A);

    // The vendor, turning them over
    float stir = sinf(firePhase4 * 1.4f) * 0.16f;
    glColor4ub(96, 62, 48, A);
    glLineWidth(5.0f);
    glBegin(GL_LINES);
        glVertex2f(x + 1.55f, y); glVertex2f(x + 1.55f, y + 1.55f);
    glEnd();
    glLineWidth(1.8f);
    glBegin(GL_LINES);
        glVertex2f(x + 1.50f, y + 1.42f); glVertex2f(x + 0.70f + stir, y + 1.86f);
    glEnd();
    FilledCircle4(x + 1.55f, y + 1.88f, 0.30f, 226, 188, 152, A);
    glColor4ub(132, 66, 54, A);
    FilledCircle4(x + 1.55f, y + 2.08f, 0.27f, 132, 66, 54, A);

    // A paper cone of them on the brazier edge
    glColor4ub(224, 208, 178, A);
    glBegin(GL_TRIANGLES);
        glVertex2f(x - 1.20f, y + 1.78f);
        glVertex2f(x - 0.72f, y + 1.78f);
        glVertex2f(x - 0.96f, y + 1.06f);
    glEnd();
    glLineWidth(1.0f);
}

// ---- Snow squall ----------------------------------------------------------
//  The old version drew four 140-unit quads sliding sideways on fmodf. Three
//  things gave it away as fake:
//    * the top edge of every band was a dead-straight horizontal line
//    * all four wrapped at the same moment, so the whole squall visibly
//      jumped every time the modulo came round
//    * they were flat lateral slabs with no internal structure, so nothing
//      ever looked like it was moving THROUGH the scene
//
//  This builds the squall out of soft overlapping blobs instead. Each one
//  carries its own drift rate, size, height and phase and wraps on its own, so
//  the mass churns continuously and never pops. A slow breathing term thickens
//  and thins the whole bank, and heavier snow settings put more of it in the
//  air.
constexpr int FOG_BLOBS4 = 15;

void DrawFog4(float y0, float y1, unsigned char alpha) {
    if (fogAmount4 < 0.01f) return;

    // Denser squall when the snow is heavier -- the two controls belong to the
    // same weather rather than being independent switches.
    float density = 0.72f + 0.20f * snowIntensity4;
    float breathe = 0.86f + 0.14f * sinf(fogPhase4 * 0.21f);
    float band    = y1 - y0;

    for (int i = 0; i < FOG_BLOBS4; i++) {
        // Stable per-blob character, so a blob keeps its identity frame to
        // frame instead of being reshuffled.
        float h1 = sinf(i * 12.9898f) * 43758.5453f;
        float h2 = sinf(i * 78.2330f) * 12345.6789f;
        float h3 = sinf(i * 45.1640f) * 27182.8182f;
        float j1 = h1 - floorf(h1);
        float j2 = h2 - floorf(h2);
        float j3 = h3 - floorf(h3);

        // Each blob has its own speed, so they shear past one another and the
        // bank never moves as one rigid sheet.
        float speed = 0.30f + 0.85f * j1;
        float span  = 150.0f;
        float cx    = fmodf(j2 * span + fogPhase4 * speed, span) - span * 0.5f;

        // Vertical drift on its own slow cycle
        float cy = y0 + band * (0.15f + 0.75f * j3)
                      + sinf(fogPhase4 * 0.13f + i * 1.7f) * band * 0.10f;

        float rx = (9.0f + 16.0f * j3) * (0.85f + 0.30f * j1);
        float ry = band * (0.16f + 0.20f * j2) * breathe;

        unsigned char a = (unsigned char)(alpha * fogAmount4 * density
                                          * (0.34f + 0.40f * j1));

        // Slightly warmer low down where the market light reaches into it,
        // colder higher up.
        float warm = 1.0f - (cy - y0) / (band + 0.001f);
        DrawSoftEllipse(cx, cy, rx, ry,
                        (unsigned char)(196 + 22 * warm),
                        (unsigned char)(208 + 12 * warm),
                        (unsigned char)(230 - 10 * warm),
                        a, 3);
    }

    // A thin veil tying the blobs together, so the bank reads as one body of
    // air rather than a row of separate clouds. Its edge is broken by a
    // shallow wave instead of being a ruled line.
    unsigned char va = (unsigned char)(alpha * fogAmount4 * 0.22f);
    glBegin(GL_QUAD_STRIP);
    for (int k = 0; k <= 40; k++) {
        float px = -62.0f + 124.0f * (float)k / 40.0f;
        float top = y0 + band * 0.74f
                  + sinf(px * 0.09f + fogPhase4 * 0.30f) * band * 0.10f
                  + sinf(px * 0.23f - fogPhase4 * 0.17f) * band * 0.05f;
        glColor4ub(200, 212, 232, 0);   glVertex2f(px, top);
        glColor4ub(200, 212, 232, va);  glVertex2f(px, y0);
    }
    glEnd();
}

void UpdateSeason4(int) {
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateSeason4, 0); return; }
    if (isAnimating4) {
        // About three seconds end to end, matching the day/night ease so the
        // two controls feel like parts of the same system.
        seasonT4 += (seasonTarget4 - seasonT4) * 0.011f;
        if (fabsf(seasonTarget4 - seasonT4) < 0.002f) seasonT4 = seasonTarget4;
    }
    glutTimerFunc(30, UpdateSeason4, 0);
}

void UpdateFog4(int) {
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateFog4, 0); return; }
    if (isAnimating4) {
        fogPhase4 += 0.13f;
        float tgt = fogMode4 ? 1.0f : 0.0f;
        fogAmount4 += (tgt - fogAmount4) * 0.02f;
    }
    glutTimerFunc(30, UpdateFog4, 0);
}

// ============================================================================
//  NEAR TERRACE -- the foreground the plaza never had
// ----------------------------------------------------------------------------
//  Everything in this scenario stood on one line: shops, stalls, tree, Santa,
//  snowmen and stage all based at y = -9, walkers at y = -12, the rink
//  bottoming out at -16.7. Below that, 23 world units -- 29% of the window --
//  was a single flat grey quad.
//
//  A low wall at y = -20 splits that dead band into a real near terrace, and
//  everything below it now belongs to the foreground: a frozen canal that
//  reflects the market, an oversized cropped lamp post, large figures seen
//  from behind, and a sledding run.
// ============================================================================
constexpr float terraceY4 = -20.0f;
constexpr float canalTopY4 = -27.0f;
constexpr float canalBotY4 = -36.0f;

// ---- Low stone wall with a snow cap --------------------------------------
void DrawTerraceWall4() {
    // Wall face
    glBegin(GL_QUADS);
        glColor3ub(96, 104, 120);
        glVertex2f(-60.0f, terraceY4 - 2.6f); glVertex2f(60.0f, terraceY4 - 2.6f);
        glColor3ub(126, 134, 150);
        glVertex2f(60.0f, terraceY4);         glVertex2f(-60.0f, terraceY4);
    glEnd();

    // Coursing
    glColor4ub(72, 80, 96, 170);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        for (int i = -30; i <= 30; i++) {
            float x = i * 4.0f;
            glVertex2f(x, terraceY4 - 2.6f); glVertex2f(x, terraceY4);
        }
        glVertex2f(-60.0f, terraceY4 - 1.3f); glVertex2f(60.0f, terraceY4 - 1.3f);
    glEnd();

    // Snow sitting on the coping, thickness following the snow cover. Out of
    // season there is none, so the cap thins to nothing.
    float cap = 0.45f * SeasonWinter4() + 1.1f * SnowDepth4();
    glColor3ub(246, 249, 255);
    glBegin(GL_QUADS);
        glVertex2f(-60.0f, terraceY4);
        glVertex2f( 60.0f, terraceY4);
        glVertex2f( 60.0f, terraceY4 + cap);
        glVertex2f(-60.0f, terraceY4 + cap);
    glEnd();

    // Drifts pushed up against the base of the wall
    glColor4ub(238, 243, 252, 220);
    for (int i = 0; i < 9; i++) {
        float cx = -54.0f + i * 13.5f + sinf(i * 2.7f) * 3.0f;
        DrawSoftEllipse(cx, terraceY4 - 2.6f, 9.0f, 1.6f * SnowDepth4() + 0.6f,
                        238, 243, 252, 200, 2);
    }
    glLineWidth(1.0f);
}

// ---- Frozen canal --------------------------------------------------------
//  The market's lights smear down its surface as vertical streaks, which is
//  what ties the foreground to the lit scene above it.
void DrawFrozenCanal4() {
    // Ice sheet
    glBegin(GL_QUADS);
        glColor3ub(MixB4(38, 90), MixB4(52, 140), MixB4(76, 160));
        glVertex2f(-60.0f, canalBotY4); glVertex2f(60.0f, canalBotY4);
        glColor3ub(MixB4(66, 120), MixB4(86, 160), MixB4(116, 190));
        glVertex2f(60.0f, canalTopY4);  glVertex2f(-60.0f, canalTopY4);
    glEnd();

    // Vertical light smears: one per source above, wobbling as the ice
    // catches them at slightly different angles.
    struct Smear { float x; unsigned char r, g, b; float w; };
    Smear smears[9] = {
        { shops4[0].x, 255, 196, 120, 2.6f },
        { shops4[1].x, 255, 196, 120, 2.4f },
        { shops4[2].x, 255, 196, 120, 2.6f },
        { shops4[3].x, 255, 196, 120, 2.2f },
        { ferrisCX4,   255, 150, 120, 4.6f },
        { treeX4,      140, 255, 170, 3.2f },
        { clockX4,     255, 210, 150, 2.4f },
        { -56.5f,      255, 170, 120, 2.4f },
        { moonX4,      200, 216, 245, 3.4f }
    };
    for (int i = 0; i < 9; i++) {
        Smear& sm = smears[i];
        unsigned char r = sm.r, g = sm.g, b = sm.b;
        if (multicolorLights4 && i < 4) { r = 220; g = 150; b = 220; }
        r = (unsigned char)(r * (0.25f + 0.75f * NightT4()));
        g = (unsigned char)(g * (0.25f + 0.75f * NightT4()));
        b = (unsigned char)(b * (0.25f + 0.75f * NightT4()));
        for (int k = 0; k < 5; k++) {
            float t   = (float)k / 5.0f;
            float y0  = canalTopY4 - t * (canalTopY4 - canalBotY4);
            float y1  = canalTopY4 - (t + 0.2f) * (canalTopY4 - canalBotY4);
            float off = sinf(twinklePhase4 * 0.9f + i * 1.7f + k * 2.4f) * 0.9f;
            float w   = sm.w * (1.0f - 0.45f * t);
            glColor4ub(r, g, b, (unsigned char)((70 * (1.0f - t)) * NightT4()));
            glBegin(GL_QUADS);
                glVertex2f(sm.x + off - w, y0);
                glVertex2f(sm.x + off + w, y0);
                glVertex2f(sm.x + off * 1.4f + w * 0.7f, y1);
                glVertex2f(sm.x + off * 1.4f - w * 0.7f, y1);
            glEnd();
        }
    }

    // Skate scars and a crack or two across the ice
    glColor4ub(190, 214, 240, 90);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        for (int i = 0; i < 14; i++) {
            float x0 = -58.0f + i * 8.3f;
            float y0 = canalTopY4 - 1.0f - (i % 4) * 2.1f;
            glVertex2f(x0, y0); glVertex2f(x0 + 6.0f + (i % 3) * 2.0f, y0 - 0.7f);
        }
    glEnd();
    glLineWidth(1.0f);
}

// ============================================================================
//  RIVERSIDE  (added on top of the baseline market/canal scene)
// ----------------------------------------------------------------------------
//  Replaces the frozen canal with a flowing river at the very front of the
//  scene, reusing its exact footprint -- riverTopY4/riverBotY4 below are
//  just the old canalTopY4/canalBotY4 -- so nothing else in the layout had
//  to move. New depth layers, back to front, matching the existing ones:
//      1. market/ground    -- unchanged
//      2. footpath         -- the existing terrace top (terraceY4), unchanged
//      3. street lights    -- the existing DrawForegroundLamp4 posts, unchanged
//      4. stairs           -- new: DrawStairsAll4()
//      5. river / boats    -- new: DrawRiver4() + DrawBoats4() +
//                             DrawGoodsUnloading4() + DrawRiverPedestrians4()
//
//  VERSION CONTROL: to undo just this addition, in Draw() swap the new
//  near-terrace block back for the single `DrawFrozenCanal4();` call it
//  replaced (that function is left intact above, just unused), remove the
//  two glutTimerFunc lines for UpdateBoats4 / UpdateRiverPedestrians4 from
//  Init(), and drop this whole RIVERSIDE section. Nothing outside it changed.
// ============================================================================
// Make the river visually wider than the old canal footprint so boats
// and dock read better in the foreground.
constexpr float riverTopY4 = canalTopY4 + 1.5f;  // far (bank) edge of the water
constexpr float riverBotY4 = canalBotY4 - 3.0f;  // near edge -- expanded deeper than the old ice
constexpr float dockY4     = riverTopY4 + 1.0f;  // narrow bank/dock ledge the stairs land on
constexpr float wallBotY4  = terraceY4 - 2.6f;   // bottom of the retaining wall (matches DrawTerraceWall4)
constexpr float stairX4[2] = { -48.0f, 24.0f };  // two staircases down to the dock

struct Boat4 {
    float x, speed, dir;        // dir/speed ignored while anchored
    float scale, phase;
    unsigned char hullR, hullG, hullB;
    bool anchored;               // sits still at the dock (the unloading boat)
    bool hasPeople;
    bool hasLantern;
};
constexpr int NUM_BOATS4 = 5;
Boat4 boats4[NUM_BOATS4] = {
    { -48.0f, 0.00f,  0, 1.05f, 0.0f, 150,  96,  58, true,  false, true  },
    { -33.0f, 0.09f,  1, 0.92f, 1.7f, 120,  74,  48, false, false, true  },
    {  -8.0f, 0.06f, -1, 0.98f, 3.1f,  96,  62,  42, false, true,  false },
    {  18.0f, 0.07f,  1, 0.90f, 4.4f, 132,  88,  54, false, true,  false },
    {  38.0f, 0.05f, -1, 1.00f, 5.6f, 108,  70,  46, false, false, true  }
};

struct RiverPed4 { float x, speed, dir, phase, laneY; unsigned char coatR, coatG, coatB; };
constexpr int NUM_RIVER_PEDS4 = 4;
RiverPed4 riverPeds4[NUM_RIVER_PEDS4] = {
    { -20.0f, 0.11f,  1, 0.0f, dockY4 + 0.2f, 150,  40,  50 },
    {   5.0f, 0.09f, -1, 1.4f, dockY4 + 0.9f,  40,  70, 120 },
    {  32.0f, 0.13f,  1, 2.6f, dockY4 + 0.2f, 190, 150,  60 },
    { -55.0f, 0.10f, -1, 3.9f, dockY4 + 0.9f,  70,  90,  80 }
};

float riverPhase4 = 0.0f;   // drives ripples, boat rocking and light flicker

// World position of a boat's hull anchor this frame (gentle rocking on top
// of whatever drift its Update function has already applied to b.x).
inline void BoatPose4(const Boat4& b, float t, float& x, float& y) {
    x = b.x;
    y = riverTopY4 - 1.2f + sinf(t * 1.2f + b.phase) * 0.28f;
}

// ---- Rowing skiff ---------------------------------------------------------
//  The old hull was a six-vertex lozenge and it did not read as a boat:
//    * the bow tip sat at y +0.25 and the stern tip at +0.35, two different
//      heights for no reason
//    * both "tips" were thin horizontal spikes poking out at mid-hull height
//    * the bottom was a dead-straight line from -2.8 to +2.6, so it stopped
//      short of both ends and the hull had no keel running into the stems
//    * no transom, no gunwale, no thwarts, no rocker
//
//  This is a proper clinker skiff instead: a keel with real rocker sweeping up
//  into a raked stem at the bow, a flat transom at the stern, a gunwale strake
//  along the sheer, two thwarts and shipped oars. `a` still carries through to
//  every part so the whole thing can be faded as one object.
void DrawBoatShape4(float x, float y, float scale,
                     unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    const int SEG = 22;

    // Sheer (gunwale line) and keel, as functions of t = -1 at the bow to
    // +1 at the stern. Both curves meet at the bow; at the stern the keel
    // stops short and the transom closes the gap.
    // sheer(t) = 0.62 + 0.34 t^2   : lowest amidships, lifting to both ends
    // keel(t)  = -0.60 + 1.22 t^2  : rocker, rising away from the middle
    const float STERN_T = 0.88f;          // where the transom cuts the hull off

    // ---- Hull ------------------------------------------------------------
    glColor4ub(r, g, b, a);
    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= SEG; i++) {
        float t  = -1.0f + (1.0f + STERN_T) * (float)i / SEG;
        float hx = x + t * 3.2f * scale;
        float sheer = ( 0.62f + 0.34f * t * t) * scale;
        float keel  = (-0.60f + 1.22f * t * t) * scale;
        if (keel > sheer) keel = sheer;                  // closed point at the bow
        glVertex2f(hx, y + sheer);
        glVertex2f(hx, y + keel);
    }
    glEnd();

    // Planking shadow along the bottom third, which is what makes a hull look
    // round rather than like a flat cut-out.
    glColor4ub((unsigned char)(r * 0.62f), (unsigned char)(g * 0.62f),
               (unsigned char)(b * 0.62f), a);
    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= SEG; i++) {
        float t  = -1.0f + (1.0f + STERN_T) * (float)i / SEG;
        float hx = x + t * 3.2f * scale;
        float sheer = ( 0.62f + 0.34f * t * t) * scale;
        float keel  = (-0.60f + 1.22f * t * t) * scale;
        if (keel > sheer) keel = sheer;
        float mid = keel + (sheer - keel) * 0.38f;
        glVertex2f(hx, y + mid);
        glVertex2f(hx, y + keel);
    }
    glEnd();

    // ---- Transom ---------------------------------------------------------
    float tx    = x + STERN_T * 3.2f * scale;
    float tTop  = y + ( 0.62f + 0.34f * STERN_T * STERN_T) * scale;
    float tBot  = y + (-0.60f + 1.22f * STERN_T * STERN_T) * scale;
    glColor4ub((unsigned char)(r * 0.80f), (unsigned char)(g * 0.80f),
               (unsigned char)(b * 0.80f), a);
    glBegin(GL_QUADS);
        glVertex2f(tx,                 tBot);
        glVertex2f(tx + 0.30f * scale, tBot + 0.10f * scale);
        glVertex2f(tx + 0.30f * scale, tTop);
        glVertex2f(tx,                 tTop);
    glEnd();

    // ---- Raked stem at the bow -------------------------------------------
    float bx = x - 3.2f * scale;
    glColor4ub((unsigned char)(r * 0.74f), (unsigned char)(g * 0.74f),
               (unsigned char)(b * 0.74f), a);
    glBegin(GL_TRIANGLES);
        glVertex2f(bx,                 y + 0.96f * scale);
        glVertex2f(bx - 0.34f * scale, y + 1.34f * scale);
        glVertex2f(bx + 0.16f * scale, y + 0.70f * scale);
    glEnd();

    // ---- Gunwale strake --------------------------------------------------
    unsigned char rr = (unsigned char)fminf(255.0f, r * 1.30f + 20.0f);
    unsigned char rg = (unsigned char)fminf(255.0f, g * 1.30f + 20.0f);
    unsigned char rb = (unsigned char)fminf(255.0f, b * 1.30f + 20.0f);
    glColor4ub(rr, rg, rb, a);
    glLineWidth(1.6f * scale);
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i <= SEG; i++) {
        float t  = -1.0f + (1.0f + STERN_T) * (float)i / SEG;
        glVertex2f(x + t * 3.2f * scale, y + (0.62f + 0.34f * t * t) * scale);
    }
    glEnd();

    // ---- Thwarts ---------------------------------------------------------
    glColor4ub((unsigned char)(r * 0.88f + 30), (unsigned char)(g * 0.88f + 26),
               (unsigned char)(b * 0.88f + 20), a);
    for (int k = 0; k < 2; k++) {
        float t  = -0.34f + k * 0.62f;
        float hx = x + t * 3.2f * scale;
        float sy = y + (0.62f + 0.34f * t * t) * scale;
        glBegin(GL_QUADS);
            glVertex2f(hx - 0.34f * scale, sy - 0.16f * scale);
            glVertex2f(hx + 0.34f * scale, sy - 0.16f * scale);
            glVertex2f(hx + 0.34f * scale, sy);
            glVertex2f(hx - 0.34f * scale, sy);
        glEnd();
    }

    // ---- Oarlock and a shipped oar ---------------------------------------
    float ox = x - 0.30f * scale;
    float oy = y + (0.62f + 0.34f * 0.0088f) * scale;
    glColor4ub(70, 62, 54, a);
    glLineWidth(1.4f * scale);
    glBegin(GL_LINES);
        glVertex2f(ox, oy); glVertex2f(ox, oy + 0.24f * scale);
    glEnd();
    glColor4ub(196, 168, 126, a);
    glLineWidth(1.8f * scale);
    glBegin(GL_LINES);
        glVertex2f(ox - 1.50f * scale, oy + 0.46f * scale);
        glVertex2f(ox + 1.70f * scale, oy + 0.10f * scale);
    glEnd();
    glColor4ub(214, 190, 150, a);
    glBegin(GL_TRIANGLES);                                // blade
        glVertex2f(ox + 1.70f * scale, oy + 0.10f * scale);
        glVertex2f(ox + 2.34f * scale, oy + 0.26f * scale);
        glVertex2f(ox + 2.30f * scale, oy - 0.10f * scale);
    glEnd();

    // ---- Waterline -------------------------------------------------------
    // A pale line where the hull meets the surface, so the boat sits IN the
    // water instead of floating above it.
    glColor4ub(232, 244, 250, (unsigned char)(a * 0.55f));
    glLineWidth(1.3f * scale);
    glBegin(GL_LINES);
        glVertex2f(x - 3.0f * scale, y - 0.30f * scale);
        glVertex2f(tx + 0.24f * scale, y - 0.30f * scale);
    glEnd();
    glLineWidth(1.0f);
}

// ---- Stairs down to the river dock ---------------------------------------
void DrawStairs4(float x) {
    const int STEPS = 6;
    // Connect the top of the stairs to the terrace so there is no gap
    // between the plaza/footpath and the first step.
    float topY = terraceY4; // previously used wallBotY4 which left a visible gap
    float botY = dockY4;
    float dropPer = (topY - botY) / STEPS;
    for (int i = 0; i < STEPS; i++) {
        float y0 = topY - i * dropPer;
        float y1 = y0 - dropPer;
        float w  = 3.2f + i * 0.12f;              // widens slightly toward the viewer
        glColor3ub(MixB4(58, 132), MixB4(60, 118), MixB4(64, 104));   // tread
        glBegin(GL_QUADS);
            glVertex2f(x - w, y1);         glVertex2f(x + w, y1);
            glVertex2f(x + w, y1 + 0.55f); glVertex2f(x - w, y1 + 0.55f);
        glEnd();
        glColor3ub(MixB4(30, 84), MixB4(32, 74), MixB4(36, 66));      // riser, in shadow
        glBegin(GL_QUADS);
            glVertex2f(x - w, y0 - 0.05f); glVertex2f(x + w, y0 - 0.05f);
            glVertex2f(x + w, y1 + 0.55f); glVertex2f(x - w, y1 + 0.55f);
        glEnd();
        float cap = 0.18f + 0.35f * SnowDepth4();                        // snow on the tread edge
        glColor4ub(246, 249, 255, 235);
        glBegin(GL_QUADS);
            glVertex2f(x - w, y1 + 0.55f);       glVertex2f(x + w, y1 + 0.55f);
            glVertex2f(x + w, y1 + 0.55f + cap); glVertex2f(x - w, y1 + 0.55f + cap);
        glEnd();
        DrawGroundShadow(x, y1, w * 0.9f, 0.0f, 55);
    }
    glColor3ub(MixB4(40, 70), MixB4(42, 68), MixB4(48, 74));
    glLineWidth(2.2f);
    glBegin(GL_LINES);
        glVertex2f(x - 3.3f, topY); glVertex2f(x - (3.2f + STEPS*0.12f), botY + 0.6f);
        glVertex2f(x + 3.3f, topY); glVertex2f(x + (3.2f + STEPS*0.12f), botY + 0.6f);
    glEnd();
    glLineWidth(1.0f);
}

void DrawStairsAll4() {
    for (int i = 0; i < 2; i++) DrawStairs4(stairX4[i]);

    // Dock ledges the stairs land on -- somewhere for a boat to tie up.
    for (int i = 0; i < 2; i++) {
        float x = stairX4[i];
        glColor3ub(MixB4(70, 132), MixB4(52, 100), MixB4(38, 76));
        glBegin(GL_QUADS);
            glVertex2f(x - 5.0f, dockY4);        glVertex2f(x + 5.0f, dockY4);
            glVertex2f(x + 5.0f, dockY4 + 1.0f); glVertex2f(x - 5.0f, dockY4 + 1.0f);
        glEnd();
        glColor4ub(30, 22, 16, 120);
        glLineWidth(1.0f);
        glBegin(GL_LINES);
            for (int p = -4; p <= 4; p++) { glVertex2f(x + p, dockY4); glVertex2f(x + p, dockY4 + 1.0f); }
        glEnd();
        float cap = 0.12f + 0.3f * SnowDepth4();
        glColor4ub(246, 249, 255, 210);
        glBegin(GL_QUADS);
            glVertex2f(x - 5.0f, dockY4 + 1.0f);       glVertex2f(x + 5.0f, dockY4 + 1.0f);
            glVertex2f(x + 5.0f, dockY4 + 1.0f + cap); glVertex2f(x - 5.0f, dockY4 + 1.0f + cap);
        glEnd();
        DrawGroundShadow(x, dockY4, 5.0f, 0.3f, 60);
    }
    glLineWidth(1.0f);
}

// ---- The river itself: base, boat reflections, ripple wash, light smears --
void DrawRiver4() {
    // Winter water runs cold blue; autumn water carries silt off the hills and
    // goes brown-green. Both still mix with the day/night value, so the four
    // combinations (winter night, winter day, autumn night, autumn day) all
    // come out of the same two lines.
    glBegin(GL_QUADS);
        glColor3ub(MixB4(MixSB4(14, 26), MixSB4(60, 74)),
                   MixB4(MixSB4(34, 46), MixSB4(96, 96)),
                   MixB4(MixSB4(52, 36), MixSB4(118, 74)));
        glVertex2f(-60.0f, riverBotY4); glVertex2f(60.0f, riverBotY4);
        glColor3ub(MixB4(MixSB4(26, 44), MixSB4(84, 104)),
                   MixB4(MixSB4(58, 62), MixSB4(128, 122)),
                   MixB4(MixSB4(86, 46), MixSB4(152, 88)));
        glVertex2f(60.0f, riverTopY4);  glVertex2f(-60.0f, riverTopY4);
    glEnd();

    // ---- No reflections ---------------------------------------------------
    //  The mirrored boats were removed earlier; the seven vertical light
    //  streaks that stood in for reflections of the wheel, the tree, the
    //  clock, the moon and the boat lanterns have now gone too. Pulling them
    //  out would have left a flat colour wash, so what replaces them is
    //  surface motion rather than surface light: cross-current ripple lines
    //  that bunch toward the far bank, a slow chop drifting downstream, and a
    //  soft foam edge where the water meets the dock.

    // ---- Cross-current ripple lines ---------------------------------------
    //  Spacing follows a squared ramp so the lines crowd together near the far
    //  bank and open out toward the camera, which is what gives a flat band of
    //  colour a sense of lying down and receding.
    const int RIPPLE_ROWS = 16;
    for (int i = 0; i < RIPPLE_ROWS; i++) {
        float u  = (float)i / (RIPPLE_ROWS - 1);        // 0 at the near edge
        float f  = 1.0f - (1.0f - u) * (1.0f - u);      // packed toward the far bank
        float y  = riverBotY4 + f * (riverTopY4 - riverBotY4);
        float amp = 0.30f * (1.0f - f) + 0.06f;         // bigger swell up close
        unsigned char al = (unsigned char)((26 + 34 * (1.0f - f)));

        glColor4ub(MixB4(150, 205), MixB4(180, 226), MixB4(206, 240), al);
        glLineWidth(1.0f + 0.7f * (1.0f - f));
        glBegin(GL_LINE_STRIP);
            for (int k = 0; k <= 40; k++) {
                float px = -62.0f + 124.0f * (float)k / 40.0f;
                float py = y
                         + sinf(px * 0.22f + riverPhase4 * 0.9f + i * 0.8f) * amp
                         + sinf(px * 0.07f - riverPhase4 * 0.5f + i * 1.9f) * amp * 0.6f;
                glVertex2f(px, py);
            }
        glEnd();
    }

    // ---- Chop: short dark troughs drifting with the current ---------------
    for (int i = 0; i < 26; i++) {
        float h1 = sinf(i * 31.77f) * 43758.5453f;
        float h2 = sinf(i * 12.13f) * 12345.6789f;
        float j1 = h1 - floorf(h1), j2 = h2 - floorf(h2);
        float f  = 0.15f + 0.80f * j2;
        float y  = riverBotY4 + f * (riverTopY4 - riverBotY4);
        float px = fmodf(j1 * 124.0f + riverPhase4 * (1.4f + 1.6f * (1.0f - f)), 124.0f) - 62.0f;
        float len = (1.4f + 2.6f * (1.0f - f));

        glColor4ub(MixB4(10, 44), MixB4(26, 78), MixB4(44, 100),
                   (unsigned char)(40 + 40 * (1.0f - f)));
        glLineWidth(1.0f + 1.1f * (1.0f - f));
        glBegin(GL_LINES);
            glVertex2f(px, y);
            glVertex2f(px + len, y + 0.10f);
        glEnd();
    }

    // ---- Foam line along the far bank -------------------------------------
    glLineWidth(1.4f);
    glBegin(GL_LINE_STRIP);
        for (int k = 0; k <= 60; k++) {
            float px = -60.0f + 120.0f * (float)k / 60.0f;
            float lace = sinf(px * 0.5f + riverPhase4 * 1.3f) * 0.10f
                       + sinf(px * 1.3f - riverPhase4 * 0.8f) * 0.05f;
            glColor4ub(MixB4(180, 232), MixB4(205, 244), MixB4(224, 250),
                       (unsigned char)(70 + 40 * sinf(px * 0.4f + riverPhase4)));
            glVertex2f(px, riverTopY4 - 0.18f + lace);
        }
    glEnd();
    glLineWidth(1.0f);

    // Small sliding glints -- what actually reads as "flowing" rather than
    // "still and lit". A handful of short bright dashes drift downstream.
    for (int i = 0; i < 8; i++) {
        float t = fmodf(i / 8.0f + riverPhase4 * 0.05f, 1.0f);
        float y = riverBotY4 + t * (riverTopY4 - riverBotY4);
        float x = fmodf(i * 23.7f + riverPhase4 * 3.0f, 120.0f) - 60.0f;
        float len = 5.0f + 3.0f * sinf(i * 1.7f);
        glColor4ub(MixB4(150, 235), MixB4(185, 245), MixB4(210, 250),
                   (unsigned char)(35 + 20 * sinf(riverPhase4 + i)));
        glLineWidth(1.3f);
        glBegin(GL_LINES); glVertex2f(x, y); glVertex2f(x + len, y + 0.15f); glEnd();
    }
    glLineWidth(1.0f);
}

// ---- Boats themselves, above the waterline --------------------------------
void DrawBoats4() {
    for (int i = 0; i < NUM_BOATS4; i++) {
        Boat4& b = boats4[i];
        float x, y;
        BoatPose4(b, riverPhase4, x, y);

        DrawGroundShadow(x, riverTopY4 + 0.2f, 2.4f * b.scale, 0.0f, 40);
        DrawBoatShape4(x, y, b.scale, b.hullR, b.hullG, b.hullB, 255);

        glColor4ub(60, 40, 26, 200);                 // thwart (bench)
        glLineWidth(1.6f * b.scale);
        glBegin(GL_LINES);
            glVertex2f(x - 0.6f*b.scale, y + 0.5f*b.scale);
            glVertex2f(x - 0.6f*b.scale, y + 0.9f*b.scale);
        glEnd();

        glColor3ub(90, 64, 40);                       // oar, dipped in the water
        glLineWidth(1.2f * b.scale);
        glBegin(GL_LINES);
            glVertex2f(x - 1.6f*b.scale, y + 0.9f*b.scale);
            glVertex2f(x - 3.6f*b.scale, y - 1.4f*b.scale);
        glEnd();

        if (b.hasPeople) {
            for (int p = 0; p < 2; p++) {
                float px = x + (p == 0 ? -0.7f : 0.9f) * b.scale;
                float py = y + 0.55f * b.scale;
                unsigned char cr = p == 0 ? 150 : 70, cg = p == 0 ? 60 : 90, cb = p == 0 ? 60 : 120;
                glColor3ub(cr, cg, cb);
                glBegin(GL_QUADS);
                    glVertex2f(px - 0.5f*b.scale, py);              glVertex2f(px + 0.5f*b.scale, py);
                    glVertex2f(px + 0.4f*b.scale, py + 1.1f*b.scale); glVertex2f(px - 0.4f*b.scale, py + 1.1f*b.scale);
                glEnd();
                FilledCircle4(px, py + 1.4f*b.scale,  0.34f*b.scale, 224, 186, 148, 255);
                FilledCircle4(px, py + 1.75f*b.scale, 0.30f*b.scale, cr, cg, cb, 255);
            }
        }

        if (b.hasLantern) {
            float lx = x + 2.6f*b.scale, lyBase = y + 0.35f*b.scale, lyTop = lyBase + 1.1f*b.scale;
            glColor3ub(40, 32, 26);
            glLineWidth(1.6f * b.scale);
            glBegin(GL_LINES); glVertex2f(lx, lyBase); glVertex2f(lx, lyTop); glEnd();
            float flick = 0.85f + 0.15f * sinf(riverPhase4 * 6.0f + b.phase * 3.0f);
            FilledCircle4(lx, lyTop, 0.28f*b.scale*flick, 255, 206, 132, (unsigned char)(230 * NightT4()));
            DrawSoftEllipse(lx, lyTop, 2.4f*b.scale, 2.4f*b.scale, 255, 196, 120, (unsigned char)(70 * NightT4()), 4);
        }
        glLineWidth(1.0f);
    }
}

// ---- Small goods-unloading vignette at the left-hand dock -----------------
void DrawGoodsUnloading4() {
    float dockX = stairX4[0] + 3.2f;
    float bx, by; BoatPose4(boats4[0], riverPhase4, bx, by);

    for (int i = 0; i < 3; i++) {                    // crates already ashore
        float cx = dockX + 1.0f + i * 1.1f;
        float cy = dockY4 + 1.0f + (i == 1 ? 1.0f : 0.0f);
        glColor3ub(120, 84, 50);
        glBegin(GL_QUADS);
            glVertex2f(cx - 0.55f, cy);          glVertex2f(cx + 0.55f, cy);
            glVertex2f(cx + 0.55f, cy + 1.0f);   glVertex2f(cx - 0.55f, cy + 1.0f);
        glEnd();
        glColor3ub(80, 54, 30);
        glLineWidth(1.4f);
        glBegin(GL_LINES); glVertex2f(cx - 0.55f, cy + 0.5f); glVertex2f(cx + 0.55f, cy + 0.5f); glEnd();
        DrawGroundShadow(cx, cy, 0.6f, 0.1f, 60);
    }

    float wx = dockX + 0.6f, wy = dockY4 + 1.0f;      // dock worker, receiving
    float reach = 0.15f * sinf(pedTimer4 * 0.5f);
    glColor3ub(64, 78, 96);
    glBegin(GL_QUADS);
        glVertex2f(wx - 0.5f, wy);          glVertex2f(wx + 0.5f, wy);
        glVertex2f(wx + 0.4f, wy + 1.6f);   glVertex2f(wx - 0.4f, wy + 1.6f);
    glEnd();
    glLineWidth(3.4f);
    glBegin(GL_LINES);
        glVertex2f(wx, wy + 1.3f); glVertex2f(wx - 1.2f + reach, wy + 1.5f);
        glVertex2f(wx, wy + 1.3f); glVertex2f(wx + 1.2f,         wy + 1.5f);
    glEnd();
    FilledCircle4(wx, wy + 1.95f, 0.34f, 224, 186, 148, 255);
    FilledCircle4(wx, wy + 2.3f,  0.28f, 60, 50, 40, 255);

    float mx = bx + 1.0f, my = by + 0.9f;             // boatman, handing a crate over
    glColor3ub(90, 50, 50);
    glBegin(GL_QUADS);
        glVertex2f(mx - 0.5f, my);          glVertex2f(mx + 0.5f, my);
        glVertex2f(mx + 0.4f, my + 1.4f);   glVertex2f(mx - 0.4f, my + 1.4f);
    glEnd();
    glLineWidth(3.0f);
    glBegin(GL_LINES); glVertex2f(mx, my + 1.1f); glVertex2f(mx + 1.3f, my + 1.3f); glEnd();
    FilledCircle4(mx, my + 1.7f, 0.32f, 224, 186, 148, 255);
    FilledCircle4(mx, my + 2.0f, 0.26f, 90, 50, 50, 255);

    float tCx = (mx + 1.3f + wx - 1.2f + reach) * 0.5f;   // the crate mid-handoff
    float tCy = (my + 1.3f + wy + 1.5f) * 0.5f;
    glColor3ub(120, 84, 50);
    glBegin(GL_QUADS);
        glVertex2f(tCx - 0.4f, tCy - 0.4f); glVertex2f(tCx + 0.4f, tCy - 0.4f);
        glVertex2f(tCx + 0.4f, tCy + 0.4f); glVertex2f(tCx - 0.4f, tCy + 0.4f);
    glEnd();
    glLineWidth(1.0f);
}

// ---- People walking the riverside dock/footpath ---------------------------
void DrawRiverPerson4(const RiverPed4& p, float t) {
    float sc = 1.35f;
    DrawGroundShadow(p.x, p.laneY, 0.7f * sc, 0.25f, 85);
    BeginDepthSprite(p.x, p.laneY, sc);
    float bob = sinf(t * 3.0f + p.phase) * 0.1f;
    float hipX = p.x, hipY = p.laneY + 1.0f + bob;
    glColor3ub(30, 30, 35);
    glLineWidth(2.6f);
    glBegin(GL_LINES);
        glVertex2f(hipX, hipY); glVertex2f(hipX + 0.24f*sinf(t*4.0f+p.phase), p.laneY);
        glVertex2f(hipX, hipY); glVertex2f(hipX - 0.24f*sinf(t*4.0f+p.phase), p.laneY);
    glEnd();
    glColor3ub(p.coatR, p.coatG, p.coatB);
    glLineWidth(5.4f);
    glBegin(GL_LINES); glVertex2f(hipX, hipY); glVertex2f(hipX, hipY + 1.2f); glEnd();
    FilledCircle4(hipX, hipY + 1.55f, 0.32f, 225, 185, 145, 255);
    glColor3ub(40, 40, 45);
    glBegin(GL_TRIANGLES);
        glVertex2f(hipX - 0.32f, hipY + 1.78f); glVertex2f(hipX + 0.32f, hipY + 1.78f);
        glVertex2f(hipX, hipY + 2.35f);
    glEnd();
    glColor4ub(246, 249, 255, 230);
    glBegin(GL_TRIANGLES);
        glVertex2f(hipX - 0.17f*SnowDepth4(), hipY + 2.1f);
        glVertex2f(hipX + 0.17f*SnowDepth4(), hipY + 2.1f);
        glVertex2f(hipX, hipY + 2.35f);
    glEnd();
    for (int i = 0; i < 3; i++) {
        float bt = fmodf(firePhase4 * 0.26f + p.phase * 0.17f + i * 0.33f, 1.0f);
        FilledCircle4(hipX + p.dir * (0.36f + bt*1.3f), hipY + 1.5f + bt*0.45f,
                      0.09f + bt*0.22f, 226, 234, 244, (unsigned char)(105 * (1.0f - bt)));
    }
    EndDepthSprite();
}

void DrawRiverPedestrians4() {
    int order[NUM_RIVER_PEDS4];
    for (int i = 0; i < NUM_RIVER_PEDS4; i++) order[i] = i;
    for (int i = 1; i < NUM_RIVER_PEDS4; i++) {
        int key = order[i], j = i - 1;
        while (j >= 0 && riverPeds4[order[j]].laneY < riverPeds4[key].laneY) { order[j+1] = order[j]; j--; }
        order[j+1] = key;
    }
    for (int i = 0; i < NUM_RIVER_PEDS4; i++) DrawRiverPerson4(riverPeds4[order[i]], pedTimer4);
}

void UpdateBoats4(int) {
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateBoats4, 0); return; }
    if (isAnimating4) {
        riverPhase4 += 0.03f;
        for (int i = 0; i < NUM_BOATS4; i++) {
            if (boats4[i].anchored) continue;
            boats4[i].x += boats4[i].speed * boats4[i].dir;
            if (boats4[i].dir > 0 && boats4[i].x > 62.0f)  boats4[i].x = -62.0f;
            if (boats4[i].dir < 0 && boats4[i].x < -62.0f) boats4[i].x = 62.0f;
        }
    }
    glutTimerFunc(30, UpdateBoats4, 0);
}

void UpdateRiverPedestrians4(int) {
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateRiverPedestrians4, 0); return; }
    if (isAnimating4) {
        for (int i = 0; i < NUM_RIVER_PEDS4; i++) {
            riverPeds4[i].x += riverPeds4[i].speed * riverPeds4[i].dir;
            if (riverPeds4[i].dir > 0 && riverPeds4[i].x > 62.0f)  riverPeds4[i].x = -62.0f;
            if (riverPeds4[i].dir < 0 && riverPeds4[i].x < -62.0f) riverPeds4[i].x = 62.0f;
        }
    }
    glutTimerFunc(30, UpdateRiverPedestrians4, 0);
}
// ============================================================================
//  END RIVERSIDE
// ============================================================================

// ---- Oversized lamp post, deliberately cropped by the frame edge ---------
//  Nothing sells depth faster than an object too close to fully fit.
void DrawForegroundLamp4(float x, float scale) {
    float baseY = terraceY4 - 2.0f;
    float topY  = baseY + 26.0f * scale;

    glColor3ub(24, 28, 38);
    glLineWidth(6.0f * scale);
    glBegin(GL_LINES); glVertex2f(x, baseY); glVertex2f(x, topY); glEnd();
    // Plinth
    glBegin(GL_QUADS);
        glVertex2f(x - 1.5f*scale, baseY);        glVertex2f(x + 1.5f*scale, baseY);
        glVertex2f(x + 1.0f*scale, baseY + 2.4f*scale); glVertex2f(x - 1.0f*scale, baseY + 2.4f*scale);
    glEnd();

    // Lantern head
    float ly = topY - 1.0f * scale;
    glColor3ub(24, 28, 38);
    glBegin(GL_QUADS);
        glVertex2f(x - 1.5f*scale, ly - 1.8f*scale); glVertex2f(x + 1.5f*scale, ly - 1.8f*scale);
        glVertex2f(x + 1.1f*scale, ly + 1.4f*scale); glVertex2f(x - 1.1f*scale, ly + 1.4f*scale);
    glEnd();
    glBegin(GL_TRIANGLES);
        glVertex2f(x - 1.7f*scale, ly + 1.4f*scale);
        glVertex2f(x + 1.7f*scale, ly + 1.4f*scale);
        glVertex2f(x,              ly + 3.0f*scale);
    glEnd();

    // Glass, and the cone of light it throws down onto the snow
    unsigned char lr = 255, lg = 206, lb = 132;
    if (multicolorLights4) { lr = 235; lg = 170; lb = 215; }
    FilledCircle4(x, ly - 0.2f*scale, 1.15f*scale, lr, lg, lb, 235);
    DrawSoftEllipse(x, ly - 0.2f*scale, 6.0f*scale, 6.0f*scale, lr, lg, lb, 60, 4);

    glColor4ub(lr, lg, lb, 30);
    glBegin(GL_QUADS);
        glVertex2f(x - 1.6f*scale, ly - 1.0f*scale);
        glVertex2f(x + 1.6f*scale, ly - 1.0f*scale);
        glVertex2f(x + 9.0f*scale, baseY);
        glVertex2f(x - 9.0f*scale, baseY);
    glEnd();

    // Snowflakes caught crossing the beam
    for (int i = 0; i < 16; i++) {
        float h1 = sinf(i * 41.7f + twinklePhase4 * 0.3f) * 43758.5453f;
        float h2 = sinf(i * 13.3f) * 12345.6789f;
        float j1 = h1 - floorf(h1), j2 = h2 - floorf(h2);
        float t  = fmodf(j2 + twinklePhase4 * 0.10f, 1.0f);
        float fy = ly - 1.0f*scale - t * (ly - 1.0f*scale - baseY);
        float spread = 1.6f + t * 7.4f;
        FilledCircle4(x + (j1 - 0.5f) * 2.0f * spread * scale, fy,
                      0.22f * scale, 255, 255, 255,
                      (unsigned char)(200 * (1.0f - t * 0.7f)));
    }

    // Pool of light where the beam lands
    DrawSoftEllipse(x, baseY, 9.5f*scale, 2.2f*scale, lr, lg, lb, 80, 4);
    glLineWidth(1.0f);
}

// Smaller plaza lamp placed on the market plaza in front of the shops.
// `baseY` is derived from the shop base so the posts sit on the plaza.
void DrawPlazaLamp4(float x, float scale) {
    float baseY = SHOP_BASE_Y4 - 2.0f; // slightly below the shop base so the lamp stands on the paving
    float topY  = baseY + 12.0f * scale; // shorter than the foreground cropped lamp

    glColor3ub(24, 28, 38);
    glLineWidth(3.0f * scale);
    glBegin(GL_LINES); glVertex2f(x, baseY); glVertex2f(x, topY); glEnd();

    // Lantern head
    float ly = topY - 0.6f * scale;
    glColor3ub(24, 28, 38);
    glBegin(GL_QUADS);
        glVertex2f(x - 0.6f*scale, ly - 0.6f*scale); glVertex2f(x + 0.6f*scale, ly - 0.6f*scale);
        glVertex2f(x + 0.5f*scale, ly + 0.8f*scale); glVertex2f(x - 0.5f*scale, ly + 0.8f*scale);
    glEnd();

    unsigned char lr = 255, lg = 206, lb = 132;
    if (multicolorLights4) { lr = 235; lg = 170; lb = 215; }
    FilledCircle4(x, ly - 0.05f*scale, 0.5f*scale, lr, lg, lb, 220);
    DrawSoftEllipse(x, ly - 0.05f*scale, 2.4f*scale, 1.2f*scale, lr, lg, lb, 48, 3);
    glLineWidth(1.0f);
}

// ---- Foreground figures, seen from behind, watching the market -----------
void DrawBackFigure4(float x, float scale,
                     unsigned char cr, unsigned char cg, unsigned char cb,
                     unsigned char hr, unsigned char hg, unsigned char hb,
                     float sway) {
    float y = terraceY4 - 2.4f;

    // Contact shadow on the snow
    DrawGroundShadow(x, y, 2.2f * scale, 0.4f, 90);

    // Coat: a simple tapered silhouette reads better than a stick figure at
    // this size, and no face is needed because they are turned away.
    glColor3ub(cr, cg, cb);
    glBegin(GL_QUADS);
        glVertex2f(x - 1.55f*scale, y);
        glVertex2f(x + 1.55f*scale, y);
        glVertex2f(x + 1.25f*scale + sway, y + 6.4f*scale);
        glVertex2f(x - 1.25f*scale + sway, y + 6.4f*scale);
    glEnd();
    // Shoulder line
    glColor4ub((unsigned char)(cr*0.75f), (unsigned char)(cg*0.75f), (unsigned char)(cb*0.75f), 255);
    glBegin(GL_QUADS);
        glVertex2f(x - 1.30f*scale + sway*0.8f, y + 5.7f*scale);
        glVertex2f(x + 1.30f*scale + sway*0.8f, y + 5.7f*scale);
        glVertex2f(x + 1.25f*scale + sway,      y + 6.4f*scale);
        glVertex2f(x - 1.25f*scale + sway,      y + 6.4f*scale);
    glEnd();

    // Boots
    glColor3ub(28, 30, 38);
    glBegin(GL_QUADS);
        glVertex2f(x - 1.3f*scale, y - 0.5f*scale); glVertex2f(x - 0.2f*scale, y - 0.5f*scale);
        glVertex2f(x - 0.2f*scale, y + 0.5f*scale); glVertex2f(x - 1.3f*scale, y + 0.5f*scale);
        glVertex2f(x + 0.2f*scale, y - 0.5f*scale); glVertex2f(x + 1.3f*scale, y - 0.5f*scale);
        glVertex2f(x + 1.3f*scale, y + 0.5f*scale); glVertex2f(x + 0.2f*scale, y + 0.5f*scale);
    glEnd();

    // Head, back of a hat, and a scarf tail lifting in the wind
    FilledCircle4(x + sway, y + 7.5f*scale, 1.05f*scale, 214, 176, 140, 255);
    FilledCircle4(x + sway, y + 8.15f*scale, 1.00f*scale, hr, hg, hb, 255);
    FilledCircle4(x + sway, y + 9.05f*scale, 0.42f*scale, 250, 250, 255, 255);
    glColor3ub(hr, hg, hb);
    glLineWidth(3.4f * scale);
    glBegin(GL_LINE_STRIP);
        glVertex2f(x + sway, y + 6.9f*scale);
        glVertex2f(x + sway - 1.6f*scale, y + 6.2f*scale);
        glVertex2f(x + sway - 2.9f*scale, y + 6.5f*scale);
    glEnd();

    // Breath, because it is cold enough for the horse to have it
    for (int i = 0; i < 3; i++) {
        float t = fmodf(firePhase4 * 0.22f + i * 0.33f, 1.0f);
        FilledCircle4(x + sway + (1.0f + t * 2.2f) * scale,
                      y + (7.4f + t * 0.7f) * scale,
                      (0.25f + t * 0.5f) * scale, 226, 234, 244,
                      (unsigned char)(95 * (1.0f - t)));
    }
    glLineWidth(1.0f);
}

void DrawForegroundCrowd4() {
    // Replace the three oversized back-facing figures on the terrace with
    // permanent ice sculptures sitting on the footpath.
    float sculptureBaseY = terraceY4 - 2.4f;
    DrawIceSculptureAt(-38.0f, sculptureBaseY, 1.15f);
    DrawIceSculptureAt(-32.5f, sculptureBaseY, 0.95f);
    DrawIceSculptureAt( 44.0f, sculptureBaseY, 1.05f);
}

// ---- Sledding run across the near terrace -------------------------------
float sledRunX4 = -70.0f;

void DrawSledRun4() {
    if (SeasonWinter4() < 0.02f) return;   // no snow on the terrace to sled down
    float x = sledRunX4;
    float y = terraceY4 - 4.6f + sinf(x * 0.09f) * 0.5f;   // gentle undulation

    // Snow spray thrown up behind the runners
    for (int i = 0; i < 8; i++) {
        float t = fmodf(firePhase4 * 0.6f + i * 0.13f, 1.0f);
        FilledCircle4(x - 2.2f - t * 5.0f, y + 0.4f + t * 1.9f,
                      0.34f * (1.0f - t), 246, 250, 255,
                      (unsigned char)(185 * (1.0f - t)));
    }

    // Sled
    glColor3ub(150, 92, 48);
    glBegin(GL_QUADS);
        glVertex2f(x - 2.0f, y);      glVertex2f(x + 2.0f, y);
        glVertex2f(x + 2.0f, y+0.7f); glVertex2f(x - 2.0f, y+0.7f);
    glEnd();
    glColor3ub(206, 214, 228);
    glLineWidth(2.6f);
    glBegin(GL_LINE_STRIP);
        glVertex2f(x - 2.2f, y - 0.15f);
        glVertex2f(x + 2.2f, y - 0.15f);
        glVertex2f(x + 2.9f, y + 0.65f);
    glEnd();

    // Child aboard, leaning into the run
    glColor3ub(200, 70, 80);
    glLineWidth(5.5f);
    glBegin(GL_LINES); glVertex2f(x - 0.4f, y + 0.7f); glVertex2f(x + 0.7f, y + 2.4f); glEnd();
    glColor3ub(30, 34, 44);
    glLineWidth(3.0f);
    glBegin(GL_LINES); glVertex2f(x - 0.4f, y + 0.9f); glVertex2f(x + 1.6f, y + 0.6f); glEnd();
    FilledCircle4(x + 0.85f, y + 3.0f, 0.72f, 226, 188, 150, 255);
    FilledCircle4(x + 0.85f, y + 3.5f, 0.62f, 70, 150, 210, 255);
    FilledCircle4(x + 0.85f, y + 4.05f, 0.26f, 250, 250, 255, 255);
    glLineWidth(1.0f);
}

void UpdateSledRun4(int) {
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateSledRun4, 0); return; }
    if (isAnimating4) {
        sledRunX4 += 0.55f;
        if (sledRunX4 > 72.0f) sledRunX4 = -72.0f - (rand() % 40);
    }
    glutTimerFunc(25, UpdateSledRun4, 0);
}

// ============================================================================
//  PLAZA GROUND / PEDESTRIANS
// ============================================================================
// ---- Plaza paving ---------------------------------------------------------
//  The old version stepped x from -56 to 56 and drew every line from (i, -6)
//  to (i - 3, -40). Because each line shifted left by exactly the same 3
//  units, all eight stayed parallel: a uniform shear, which the eye reads as
//  a tilted wall rather than a floor going away from you. There were also no
//  cross-lines at all, so nothing established scale.
//
//  Now the courses radiate from a vanishing point on the horizon and the
//  horizontal joints compress toward it, which is what actually makes a
//  ground plane recede.
constexpr float plazaVanishX4 = 0.0f;
constexpr float plazaHorizonY4 = -6.0f;
constexpr float plazaNearY4    = -40.0f;

// Maps a 0..1 "distance from the camera" onto a world y, packing the courses
// closer together as they approach the horizon.
inline float PlazaCourseY4(float t) {
    float k = 1.0f - (1.0f - t) * (1.0f - t);          // ease toward the horizon
    return plazaNearY4 + (plazaHorizonY4 - plazaNearY4) * k;
}

void DrawPlazaGround4() {
    // Winter: snow-lit paving, cold and blue near the camera, picking up the
    // town glow at the far edge. Autumn: the snow is gone and the same stone
    // shows through, damp and ochre.
    glBegin(GL_QUADS);
        glColor3ub(MixSB4(198, 128), MixSB4(210, 116), MixSB4(226, 102));
        glVertex2f(-60, plazaNearY4); glVertex2f(60, plazaNearY4);
        glColor3ub(MixSB4(228, 164), MixSB4(233, 148), MixSB4(240, 126));
        glVertex2f(60, plazaHorizonY4);   glVertex2f(-60, plazaHorizonY4);
    glEnd();

    glColor4ub(MixSB4(178, 96), MixSB4(190, 86), MixSB4(208, 74), 150);
    glLineWidth(1.0f);

    // Courses running away from the camera, fanning out from the vanishing
    // point so they converge properly at the horizon.
    glBegin(GL_LINES);
    for (int i = -9; i <= 9; i++) {
        float xTop  = plazaVanishX4 + i * 6.5f;                       // at the horizon
        float xNear = plazaVanishX4 + (xTop - plazaVanishX4) * 3.6f;  // spread out near
        glVertex2f(xTop,  plazaHorizonY4);
        glVertex2f(xNear, plazaNearY4);
    }
    glEnd();

    // Cross joints, spaced so they bunch up toward the horizon.
    glBegin(GL_LINES);
    for (int c = 1; c <= 9; c++) {
        float t = (float)c / 10.0f;
        float y = PlazaCourseY4(t);
        glVertex2f(-60.0f, y);
        glVertex2f( 60.0f, y);
    }
    glEnd();

    // Trodden snow: a broad scuffed path worn between the shops and the
    // rink, wider where it is nearer the camera. In autumn the same route is
    // a wet patch rather than a pale one, so it darkens instead of vanishing.
    glColor4ub(MixSB4(236, 108), MixSB4(241, 98), MixSB4(248, 84), 120);
    glBegin(GL_QUADS);
        glVertex2f(-16.0f, plazaNearY4); glVertex2f(30.0f, plazaNearY4);
        glVertex2f( 10.0f, plazaHorizonY4); glVertex2f(-4.0f, plazaHorizonY4);
    glEnd();

    // ---- Fallen leaves, drifted across the paving -------------------------
    // Only in autumn, and only settled ones: the falling leaves are the snow
    // particle bands doing double duty (see DrawSnowBand4).
    if (SeasonAutumn4() > 0.02f) {
        const unsigned char leafCols[4][3] = {
            {168, 82, 34}, {196, 128, 42}, {140, 62, 40}, {186, 156, 58}
        };
        for (int i = 0; i < 150; i++) {
            float h1 = sinf(i * 12.9898f) * 43758.5453f;
            float h2 = sinf(i * 78.2330f) * 12345.6789f;
            float h3 = sinf(i * 45.1640f) * 27182.8182f;
            float j1 = h1 - floorf(h1), j2 = h2 - floorf(h2), j3 = h3 - floorf(h3);

            // Denser toward the camera, where the paving is bigger on screen.
            float f  = j2 * j2;
            float ly = plazaHorizonY4 + (plazaNearY4 - plazaHorizonY4) * f;
            float lx = -62.0f + j1 * 124.0f;
            float sz = 0.16f + 0.42f * f;
            const unsigned char* c = leafCols[i % 4];
            glColor4ub(c[0], c[1], c[2], AutumnA4(200));
            glBegin(GL_TRIANGLES);
                float a = j3 * 6.2831853f;
                glVertex2f(lx + sz * cosf(a),          ly + sz * 0.45f * sinf(a));
                glVertex2f(lx + sz * cosf(a + 2.3f),   ly + sz * 0.45f * sinf(a + 2.3f));
                glVertex2f(lx + sz * cosf(a + 4.1f),   ly + sz * 0.45f * sinf(a + 4.1f));
            glEnd();
        }
    }
    glLineWidth(1.0f);
}

// ============================================================================
//  PERSPECTIVE ROAD
// ----------------------------------------------------------------------------
//  A side street opening in the gap between the corner shop and TOYS, running
//  out of the distance and sweeping away to the left as it reaches the camera.
//
//  Three things make it read as a road going back rather than a pale trapezoid
//  lying on the snow:
//
//    * width. RoadHW4() grows from under two units at the far end to a dozen
//      at the near end, so the carriageway converges hard toward the gap;
//    * curvature. RoadCX4() pulls the centreline left as it comes forward, so
//      the road turns out of the frame instead of pointing at the viewer. A
//      straight road drawn in perspective still reads flat; a curved one does
//      not;
//    * spacing. RoadY4() packs the courses together toward the horizon, which
//      is what a constant-length stretch of road does as it recedes.
//
//  It is drawn straight after the plaza so it is part of the ground: every
//  stall, figure, lamp and the ferris wheel itself all come later and sit on
//  top of it, exactly as they sit on top of the snow.
// ============================================================================
constexpr float roadNearY4 = -20.4f;    // the terrace coping crops it here

// 0 at the horizon, 1 at the near end.
inline float RoadY4 (float t) { return roadFarY4 + (roadNearY4 - roadFarY4) * (0.42f * t + 0.58f * t * t); }
inline float RoadCX4(float t) { return roadFarX4 - 19.2f * powf(t, 1.70f); }
inline float RoadHW4(float t) { return roadFarHW4 + 10.2f * powf(t, 1.28f); }

// Packed-snow road surface at a given point along the run. `side` is -1 on the
// left kerb and +1 on the right: the buildings on the right of the street are
// between the road and the light, so that half sits in their shade.
inline void RoadSurfaceColour4(float t, float y, float side,
                               unsigned char& r, unsigned char& g, unsigned char& b) {
    float depth = 0.86f * (1.0f - t);
    // Packed, part-cleared snow rather than asphalt: this is a winter town, and
    // a genuinely dark ribbon here reads as a hole cut in the plaza instead of
    // as a street. Still clearly darker and greyer than the untouched snow
    // either side, which is what makes the road legible at a glance.
    int nr = 104, ng = 116, nb = 142, dr = 142, dg = 152, db = 170;
    if (side > 0.0f) { nr = 78; ng = 88; nb = 114; dr = 112; dg = 122; db = 142; }
    AirFade4(depth, y, nr, ng, nb, dr, dg, db, r, g, b);
}

void DrawPerspectiveRoad4() {
    const int N = 30;

    // ---- Ploughed verges ---------------------------------------------------
    // Banked snow either side, drawn first and slightly wider than the
    // carriageway, so the road sits down INTO the snow instead of being
    // painted on top of it.
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= N; i++) {
        float t  = (float)i / N;
        float y  = RoadY4(t), cx = RoadCX4(t), hw = RoadHW4(t);
        float bank = 0.6f + 2.4f * t;
        unsigned char br, bg, bb;
        AirFade4(0.85f * (1.0f - t), y, 224, 232, 246, 246, 250, 255, br, bg, bb);
        glColor3ub(br, bg, bb);
        glVertex2f(cx - hw - bank, y);
        glVertex2f(cx + hw + bank, y);
    }
    glEnd();

    // ---- Carriageway -------------------------------------------------------
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= N; i++) {
        float t  = (float)i / N;
        float y  = RoadY4(t), cx = RoadCX4(t), hw = RoadHW4(t);
        unsigned char lr, lg, lb, rr, rg, rb;
        RoadSurfaceColour4(t, y, -1.0f, lr, lg, lb);
        RoadSurfaceColour4(t, y,  1.0f, rr, rg, rb);
        glColor3ub(lr, lg, lb); glVertex2f(cx - hw, y);
        glColor3ub(rr, rg, rb); glVertex2f(cx + hw, y);
    }
    glEnd();

    // ---- Wheel ruts --------------------------------------------------------
    // Two darker ribbons following the same curve. They are the clearest
    // signal that the surface is a road and not a frozen pond.
    for (int lane = -1; lane <= 1; lane += 2) {
        glBegin(GL_QUAD_STRIP);
        for (int i = 0; i <= N; i++) {
            float t  = (float)i / N;
            float y  = RoadY4(t), cx = RoadCX4(t), hw = RoadHW4(t);
            float c  = cx + lane * hw * 0.44f;
            float w  = hw * 0.17f;
            glColor4ub(MixB4(52, 104), MixB4(60, 112), MixB4(82, 130),
                       (unsigned char)(105.0f * (0.20f + 0.80f * t)));
            glVertex2f(c - w, y);
            glVertex2f(c + w, y);
        }
        glEnd();
    }

    // ---- Kerb lines --------------------------------------------------------
    // A bright snow lip on each edge: a hard boundary is what tells the eye
    // where the road surface stops and the verge begins.
    for (int side = -1; side <= 1; side += 2) {
        // The bright lip itself...
        glBegin(GL_QUAD_STRIP);
        for (int i = 0; i <= N; i++) {
            float t  = (float)i / N;
            float y  = RoadY4(t), cx = RoadCX4(t), hw = RoadHW4(t);
            float lip = 0.16f + 0.85f * t;
            glColor4ub(MixB4(228, 250), MixB4(238, 252), MixB4(250, 255),
                       (unsigned char)(230.0f * (0.40f + 0.60f * t)));
            glVertex2f(cx + side * hw, y);
            glVertex2f(cx + side * (hw + lip), y);
        }
        glEnd();
        // ...and the shadow the banked snow drops onto the carriageway beside
        // it. A bright line alone floats; a bright line with a dark line under
        // it reads as a raised kerb.
        glBegin(GL_QUAD_STRIP);
        for (int i = 0; i <= N; i++) {
            float t  = (float)i / N;
            float y  = RoadY4(t), cx = RoadCX4(t), hw = RoadHW4(t);
            float sh = 0.10f + 0.65f * t;
            glColor4ub(MixB4(24, 74), MixB4(30, 84), MixB4(52, 110),
                       (unsigned char)(MixB4(120, 80) * (0.30f + 0.70f * t)));
            glVertex2f(cx + side * hw, y);
            glColor4ub(MixB4(24, 74), MixB4(30, 84), MixB4(52, 110), 0);
            glVertex2f(cx + side * (hw - sh), y);
        }
        glEnd();
    }

    // ---- Centre markings ---------------------------------------------------
    // Only the near half: further back the dashes would be sub-pixel, and
    // showing them anyway is the classic way to flatten a perspective road.
    for (int d = 0; d < 7; d++) {
        float t0 = 0.34f + d * 0.095f;
        float t1 = t0 + 0.045f;
        if (t1 > 1.0f) break;
        float y0 = RoadY4(t0), y1 = RoadY4(t1);
        float c0 = RoadCX4(t0), c1 = RoadCX4(t1);
        float w0 = RoadHW4(t0) * 0.035f, w1 = RoadHW4(t1) * 0.035f;
        // Snow keeps covering and uncovering them, so a couple are missing.
        if (d == 2 || d == 5) continue;
        glColor4ub(MixB4(196, 226), MixB4(186, 214), MixB4(150, 178),
                   (unsigned char)(150.0f * t0));
        glBegin(GL_QUADS);
            glVertex2f(c0 - w0, y0); glVertex2f(c0 + w0, y0);
            glVertex2f(c1 + w1, y1); glVertex2f(c1 - w1, y1);
        glEnd();
    }

    // ---- Shade thrown across the road by the buildings on its right --------
    // The light sits up and to the right, so the right-hand terrace lays a
    // wedge of shadow over the carriageway that narrows with distance.
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= N; i++) {
        float t  = (float)i / N;
        float y  = RoadY4(t), cx = RoadCX4(t), hw = RoadHW4(t);
        float reach = hw * (0.85f - 0.35f * t);
        glColor4ub(MixB4(18, 58), MixB4(24, 68), MixB4(44, 92),
                   (unsigned char)(MixB4(58, 48) * (0.35f + 0.65f * t)));
        glVertex2f(cx + hw, y);
        glColor4ub(MixB4(8, 44), MixB4(10, 52), MixB4(20, 74), 0);
        glVertex2f(cx + hw - reach, y);
    }
    glEnd();

    // ---- Drifts caught against the verges -----------------------------------
    for (int i = 0; i < 7; i++) {
        float t  = 0.18f + i * 0.13f;
        if (t > 1.0f) break;
        float y  = RoadY4(t), cx = RoadCX4(t), hw = RoadHW4(t);
        float sc = 0.35f + 0.95f * t;
        float side = (i % 2 == 0) ? -1.0f : 1.0f;
        DrawSoftEllipse(cx + side * (hw + 1.9f * sc), y, 2.2f * sc,
                        (0.35f + 0.55f * SnowDepth4()) * sc,
                        244, 248, 254, 215, 2);
    }

    // ---- Night lighting on the surface --------------------------------------
    // Warm pools from the street's own windows and the market spilling in at
    // the near end, so the road is lit by the scene rather than by nothing.
    float night = NightT4();
    if (night > 0.02f) {
        for (int i = 0; i < 4; i++) {
            float t  = 0.10f + i * 0.27f;
            float y  = RoadY4(t), cx = RoadCX4(t), hw = RoadHW4(t);
            DrawSoftEllipse(cx + hw * 0.35f, y, hw * 0.95f, 0.9f + 1.1f * t,
                            255, 202, 140, (unsigned char)((18 + 16 * t) * night), 4);
        }
    }
    // By day the open street is the brightest strip of ground in the frame.
    if (dayT4 > 0.02f) {
        float y = RoadY4(0.62f), cx = RoadCX4(0.62f), hw = RoadHW4(0.62f);
        DrawSoftEllipse(cx, y, hw * 1.5f, 5.5f, 250, 246, 226,
                        (unsigned char)(52.0f * dayT4), 4);
    }

    // Where the road meets the horizon it should dissolve, not butt into the
    // buildings with a visible seam.
    float hr, hg, hb;
    SkyAirColour4(-5.0f, hr, hg, hb);
    glBegin(GL_QUADS);
        glColor4ub((unsigned char)hr, (unsigned char)hg, (unsigned char)hb, MixB4(210, 230));
        glVertex2f(RoadCX4(0.0f) - RoadHW4(0.0f) - 1.2f, roadFarY4);
        glVertex2f(RoadCX4(0.0f) + RoadHW4(0.0f) + 1.2f, roadFarY4);
        glColor4ub((unsigned char)hr, (unsigned char)hg, (unsigned char)hb, 0);
        glVertex2f(RoadCX4(0.22f) + RoadHW4(0.22f), RoadY4(0.22f));
        glVertex2f(RoadCX4(0.22f) - RoadHW4(0.22f), RoadY4(0.22f));
    glEnd();
}

// ---- Shadows the buildings lay on the snow --------------------------------
//  Every mass in the scene was previously floating on an unbroken white
//  plaza. The light is up and to the right in both modes, so each facade
//  throws a soft wedge down and to the left of itself. They are deliberately
//  weak: the point is to anchor the row to the ground, not to darken it.
// ============================================================================
//  CYCLE RICKSHAW
// ----------------------------------------------------------------------------
//  The perspective street was built and then left empty. This puts a
//  three-wheeler on it, and takes its position AND its size straight from the
//  road's own functions -- RoadCX4/RoadY4/RoadHW4 -- so it tracks the curve and
//  grows at exactly the rate the carriageway does. Nothing about it is
//  hand-tuned against the road; change the road and the rickshaw follows.
//
//  Three wheels, and all three are meant to be legible: one steered wheel at
//  the front, then a rear axle whose near wheel is drawn full strength and
//  whose far wheel is smaller, darker and offset, so the pair does not
//  collapse into a single bicycle wheel.
// ============================================================================
//  PARKED. Unlike the car and the bicycle, this one does not run: it is stood
//  off the carriageway near the bottom of the street, waiting for a fare.
//  Because it is stationary it is still drawn in profile -- that is how a
//  parked vehicle at a kerb is actually seen, and the side view is what makes
//  it read as waiting rather than as traffic.
constexpr float RICKSHAW_PARK_T4 = 0.80f;   // how far down the street it waits

float rickshawT4     = RICKSHAW_PARK_T4;  // fixed: it never moves
float rickshawWheel4 = 0.0f;              // fixed: the wheels never turn

void DrawRickshaw4() {
    const float t  = rickshawT4;
    const float y  = RoadY4(t);
    const float cx = RoadCX4(t);

    // Size from the road's own width: at the near end the carriageway is about
    // seven times wider than at the horizon, and the rickshaw grows with it.
    const float S  = 0.30f + 1.25f * powf(t, 1.28f);

    // Stood well past the right-hand kerb, on the verge. The offset is 1.62
    // rather than something closer to 1.0 because the street CURVES: the
    // cyclist keeps to 0.72 of the half-width, but further up the road that
    // line sits further right in world x than the kerb does down here, so a
    // rickshaw parked just off the kerb still gets ridden through. 1.62
    // clears the cyclist's whole swept path and still leaves the tea stall
    // (x = -23) a couple of units of air.
    const float x  = cx + RoadHW4(t) * 1.62f;

    // Everything further away is hazier: the same air the rest of the street
    // is fading through.
    const float depth = 0.80f * (1.0f - t);

    unsigned char r, g, b;

    DrawGroundShadow(x, y, 2.4f * S, 0.35f, (unsigned char)(90 * (0.35f + 0.65f * t)));

    // ---- Far rear wheel ---------------------------------------------------
    // Drawn first, smaller and darker: it is on the other side of the axle.
    AirFade4(depth, y, 28, 30, 38, 74, 80, 92, r, g, b);
    glColor3ub(r, g, b);
    FilledCircle4(x - 1.30f * S, y + 0.92f * S, 0.86f * S, r, g, b, 255);
    AirFade4(depth, y, 52, 56, 66, 104, 110, 124, r, g, b);
    FilledCircle4(x - 1.30f * S, y + 0.92f * S, 0.60f * S, r, g, b, 255);

    // ---- Rear axle --------------------------------------------------------
    AirFade4(depth, y, 46, 48, 56, 96, 100, 112, r, g, b);
    glColor3ub(r, g, b);
    glLineWidth(2.0f * S);
    glBegin(GL_LINES);
        glVertex2f(x - 1.30f * S, y + 0.92f * S);
        glVertex2f(x - 0.55f * S, y + 0.86f * S);
    glEnd();

    // ---- Passenger cab ----------------------------------------------------
    // Body
    AirFade4(depth, y, 104, 34, 44, 186, 66, 74, r, g, b);
    glColor3ub(r, g, b);
    glBegin(GL_POLYGON);
        glVertex2f(x - 2.10f * S, y + 1.05f * S);
        glVertex2f(x + 0.30f * S, y + 1.05f * S);
        glVertex2f(x + 0.42f * S, y + 2.30f * S);
        glVertex2f(x - 2.16f * S, y + 2.30f * S);
    glEnd();
    // Panel line and a chrome strip, so the cab is not one flat block
    AirFade4(depth, y, 70, 22, 30, 132, 44, 52, r, g, b);
    glColor3ub(r, g, b);
    glLineWidth(1.4f * S);
    glBegin(GL_LINES);
        glVertex2f(x - 2.10f * S, y + 1.62f * S);
        glVertex2f(x + 0.34f * S, y + 1.62f * S);
    glEnd();
    AirFade4(depth, y, 150, 152, 160, 206, 208, 214, r, g, b);
    glColor3ub(r, g, b);
    glBegin(GL_LINES);
        glVertex2f(x - 2.12f * S, y + 1.05f * S);
        glVertex2f(x + 0.32f * S, y + 1.05f * S);
    glEnd();

    // Bench seat back
    AirFade4(depth, y, 58, 40, 30, 112, 82, 58, r, g, b);
    glColor3ub(r, g, b);
    glBegin(GL_QUADS);
        glVertex2f(x - 2.00f * S, y + 2.30f * S);
        glVertex2f(x + 0.20f * S, y + 2.30f * S);
        glVertex2f(x + 0.20f * S, y + 2.74f * S);
        glVertex2f(x - 2.00f * S, y + 2.74f * S);
    glEnd();

    // ---- Empty cab --------------------------------------------------------
    // No passenger: it is waiting FOR a fare, so the bench is empty and the
    // shadow inside the hood is all that shows.
    AirFade4(depth, y, 24, 20, 22, 58, 48, 50, r, g, b);
    glColor3ub(r, g, b);
    glBegin(GL_QUADS);
        glVertex2f(x - 1.94f * S, y + 2.40f * S);
        glVertex2f(x + 0.14f * S, y + 2.40f * S);
        glVertex2f(x + 0.14f * S, y + 2.74f * S);
        glVertex2f(x - 1.94f * S, y + 2.74f * S);
    glEnd();

    // ---- Folding hood -----------------------------------------------------
    // A half-dome over the bench, with its ribs showing.
    AirFade4(depth, y, 32, 34, 42, 62, 66, 78, r, g, b);
    glColor3ub(r, g, b);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(x - 0.90f * S, y + 2.74f * S);
        for (int i = 0; i <= 12; i++) {
            float a = PI4 * ((float)i / 12.0f);
            glVertex2f(x - 0.90f * S + 1.42f * S * cosf(a),
                       y + 2.74f * S + 2.00f * S * sinf(a));
        }
    glEnd();
    AirFade4(depth, y, 58, 60, 70, 96, 100, 112, r, g, b);
    glColor3ub(r, g, b);
    glLineWidth(1.2f * S);
    glBegin(GL_LINES);
        for (int i = 1; i < 4; i++) {
            float a = PI4 * ((float)i / 4.0f);
            glVertex2f(x - 0.90f * S, y + 2.74f * S);
            glVertex2f(x - 0.90f * S + 1.42f * S * cosf(a),
                       y + 2.74f * S + 2.00f * S * sinf(a));
        }
    glEnd();

    // Snow settled along the top of the hood
    glColor4ub(246, 249, 255, (unsigned char)(235 * SnowDepth4()));
    glLineWidth(2.6f * S);
    glBegin(GL_LINE_STRIP);
        for (int i = 2; i <= 10; i++) {
            float a = PI4 * ((float)i / 12.0f);
            glVertex2f(x - 0.90f * S + 1.46f * S * cosf(a),
                       y + 2.74f * S + 2.04f * S * sinf(a));
        }
    glEnd();

    // ---- Frame down to the front fork -------------------------------------
    AirFade4(depth, y, 46, 48, 56, 96, 100, 112, r, g, b);
    glColor3ub(r, g, b);
    glLineWidth(2.2f * S);
    glBegin(GL_LINE_STRIP);
        glVertex2f(x - 0.55f * S, y + 0.86f * S);
        glVertex2f(x + 0.55f * S, y + 1.16f * S);
        glVertex2f(x + 1.70f * S, y + 1.90f * S);   // steering head
        glVertex2f(x + 1.86f * S, y + 0.84f * S);   // fork down to the hub
    glEnd();

    // Handlebars
    AirFade4(depth, y, 46, 48, 56, 96, 100, 112, r, g, b);
    glColor3ub(r, g, b);
    glLineWidth(1.6f * S);
    glBegin(GL_LINES);
        glVertex2f(x + 1.52f * S, y + 2.18f * S);
        glVertex2f(x + 1.98f * S, y + 2.06f * S);
    glEnd();

    // ---- Near rear wheel and front wheel ----------------------------------
    // Drawn last so their spokes sit over the frame.
    for (int w = 0; w < 2; w++) {
        float wx = (w == 0) ? (x - 0.55f * S) : (x + 1.86f * S);
        float wy = (w == 0) ? (y + 0.86f * S) : (y + 0.84f * S);
        float wr = (w == 0) ? (0.90f * S)     : (0.86f * S);

        AirFade4(depth, y, 22, 24, 30, 58, 62, 74, r, g, b);
        FilledCircle4(wx, wy, wr, r, g, b, 255);
        AirFade4(depth, y, 70, 74, 84, 128, 132, 146, r, g, b);
        FilledCircle4(wx, wy, wr * 0.70f, r, g, b, 255);
        AirFade4(depth, y, 30, 32, 40, 66, 70, 82, r, g, b);
        FilledCircle4(wx, wy, wr * 0.22f, r, g, b, 255);

        // Spokes, turning with the distance travelled
        AirFade4(depth, y, 96, 100, 110, 156, 160, 172, r, g, b);
        glColor3ub(r, g, b);
        glLineWidth(1.0f);
        glBegin(GL_LINES);
            for (int k = 0; k < 6; k++) {
                float a = rickshawWheel4 + k * (PI4 / 6.0f);
                glVertex2f(wx - wr * 0.68f * cosf(a), wy - wr * 0.68f * sinf(a));
                glVertex2f(wx + wr * 0.68f * cosf(a), wy + wr * 0.68f * sinf(a));
            }
        glEnd();
    }

    // ---- Driver -----------------------------------------------------------
    // Waiting, not working: he is stood astride the frame with both feet on
    // the road and his back straight, rather than up on the pedals leaning
    // into it. A slow idle sway keeps him from looking like a frozen frame.
    const float sway = sinf(pedTimer4 * 0.62f) * 0.07f * S;

    AirFade4(depth, y, 60, 52, 44, 104, 92, 78, r, g, b);
    glColor3ub(r, g, b);
    glLineWidth(1.6f * S);
    glBegin(GL_LINES);                                   // both legs planted
        glVertex2f(x + 0.92f * S, y + 2.10f * S);
        glVertex2f(x + 0.58f * S, y + 0.10f * S);
        glVertex2f(x + 1.04f * S, y + 2.10f * S);
        glVertex2f(x + 1.42f * S, y + 0.10f * S);
    glEnd();
    // Torso, near upright
    AirFade4(depth, y, 52, 74, 58, 92, 132, 100, r, g, b);
    glColor3ub(r, g, b);
    glLineWidth(4.6f * S);
    glBegin(GL_LINES);
        glVertex2f(x + 0.98f * S, y + 2.10f * S);
        glVertex2f(x + 1.16f * S + sway, y + 3.24f * S);
    glEnd();
    // One arm resting on the handlebars, the other hanging at his side
    glLineWidth(1.8f * S);
    glBegin(GL_LINES);
        glVertex2f(x + 1.14f * S + sway, y + 3.06f * S);
        glVertex2f(x + 1.76f * S, y + 2.20f * S);
        glVertex2f(x + 1.10f * S + sway, y + 3.02f * S);
        glVertex2f(x + 0.92f * S, y + 2.06f * S);
    glEnd();
    AirFade4(depth, y, 150, 122, 100, 226, 188, 152, r, g, b);
    FilledCircle4(x + 1.22f * S + sway, y + 3.56f * S, 0.30f * S, r, g, b, 255);
    AirFade4(depth, y, 100, 60, 50, 176, 106, 88, r, g, b);
    FilledCircle4(x + 1.22f * S + sway, y + 3.76f * S, 0.27f * S, r, g, b, 255);

    // ---- Lamp -------------------------------------------------------------
    // Lit at night, and it throws a patch of light onto the road ahead.
    float lampOn = NightT4();
    if (lampOn > 0.05f) {
        // Parked, so it is a sidelight, not a driving beam: a small halo and
        // a patch on the verge under it rather than a wash thrown up the
        // road. At the old size the halo swallowed the whole front wheel.
        float lx = x + 2.10f * S, ly = y + 2.00f * S;
        DrawSoftEllipse(lx + 0.4f * S, y + 0.25f * S, 2.0f * S, 0.7f * S,
                        255, 222, 158, (unsigned char)(62 * lampOn), 3);
        FilledCircle4(lx, ly, 0.34f * S, 255, 216, 150, (unsigned char)(70 * lampOn));
        FilledCircle4(lx, ly, 0.16f * S, 255, 246, 214, (unsigned char)(255 * lampOn));
    }
    glLineWidth(1.0f);
}

// ============================================================================
//  ROADSIDE TEA STALL
// ----------------------------------------------------------------------------
//  The band between the road's near kerb and the ice rink -- roughly
//  x -34 .. -12 on y -16 .. -20 -- was open snow with nothing on it, right in
//  the front third of the frame where the eye spends most of its time.
//
//  A tea stall fills it with the one thing the plaza did not have: people
//  sitting still. Everything else in this scene is walking, skating, turning
//  or flying, so a group settled on benches with cups in their hands gives the
//  whole square somewhere to rest.
// ============================================================================
constexpr float TEA_X4 = -23.0f;
constexpr float TEA_Y4 = -18.6f;
// 1.10 rather than the 1.28 first tried: at the larger size the top of the
// awning reached y = -12.1, exactly the ground line of the market stalls
// behind it, and the two crowded each other. This keeps a clear unit of air
// between them while the stall still reads as a big foreground object.
constexpr float TEA_S4 =  1.10f;

// One seated customer, cup in hand, steam rising off it.
void DrawTeaSitter4(float x, float y, float sc, int seed,
                    unsigned char cr, unsigned char cg, unsigned char cb,
                    bool facingLeft) {
    const float d = facingLeft ? -1.0f : 1.0f;
    float lean = sinf(pedTimer4 * 0.6f + seed * 1.7f) * 0.05f;
    float sip  = sinf(pedTimer4 * 0.9f + seed * 2.3f);
    bool  drinking = (sip > 0.72f);              // every so often, a mouthful

    DrawGroundShadow(x, y, 0.86f * sc, 0.22f, 78);

    // Lower legs down from the bench, feet on the snow
    glColor3ub(38, 38, 46);
    glLineWidth(2.4f * sc);
    glBegin(GL_LINES);
        glVertex2f(x - 0.16f * sc, y + 1.06f * sc); glVertex2f(x - 0.16f * sc, y);
        glVertex2f(x + 0.20f * sc, y + 1.06f * sc); glVertex2f(x + 0.20f * sc, y);
    glEnd();
    // Thighs along the bench
    glColor3ub(cr, cg, cb);
    glLineWidth(4.0f * sc);
    glBegin(GL_LINES);
        glVertex2f(x + 0.02f * sc, y + 1.10f * sc);
        glVertex2f(x - d * 0.60f * sc, y + 1.14f * sc);
    glEnd();
    // Torso, hunched a little over the cup
    glLineWidth(5.2f * sc);
    glBegin(GL_LINES);
        glVertex2f(x + 0.02f * sc, y + 1.10f * sc);
        glVertex2f(x + (0.10f + lean) * sc, y + 2.14f * sc);
    glEnd();

    // Arm bringing the cup up
    float cupY = y + (drinking ? 2.18f : 1.86f) * sc;
    float cupX = x + (0.10f + lean) * sc + d * 0.34f * sc;
    glColor3ub((unsigned char)(cr * 0.82f), (unsigned char)(cg * 0.82f),
               (unsigned char)(cb * 0.82f));
    glLineWidth(2.2f * sc);
    glBegin(GL_LINES);
        glVertex2f(x + (0.08f + lean) * sc, y + 1.94f * sc);
        glVertex2f(cupX, cupY);
    glEnd();

    // Head and woolly hat
    FilledCircle4(x + (0.10f + lean) * sc, y + 2.46f * sc, 0.30f * sc, 228, 190, 154, 255);
    FilledCircle4(x + (0.10f + lean) * sc, y + 2.66f * sc, 0.28f * sc, cr, cg, cb, 255);
    FilledCircle4(x + (0.10f + lean) * sc, y + 2.86f * sc, 0.11f * sc, 246, 246, 250, 255);

    // The cup itself, and the steam off it
    glColor3ub(238, 238, 242);
    glBegin(GL_QUADS);
        glVertex2f(cupX - 0.15f * sc, cupY - 0.16f * sc);
        glVertex2f(cupX + 0.15f * sc, cupY - 0.16f * sc);
        glVertex2f(cupX + 0.13f * sc, cupY + 0.14f * sc);
        glVertex2f(cupX - 0.13f * sc, cupY + 0.14f * sc);
    glEnd();
    for (int i = 0; i < 3; i++) {
        float t = fmodf(firePhase4 * 0.30f + seed * 0.2f + i * 0.33f, 1.0f);
        FilledCircle4(cupX + sinf(t * 4.0f + seed) * 0.14f * sc,
                      cupY + (0.22f + t * 0.80f) * sc,
                      (0.06f + t * 0.13f) * sc, 232, 238, 246,
                      (unsigned char)(130 * (1.0f - t)));
    }
    glLineWidth(1.0f);
}

void DrawTeaStall4() {
    const float x = TEA_X4, y = TEA_Y4, S = TEA_S4;

    // ---- Warm pool of light on the snow underneath it ---------------------
    DrawGroundGlow4(x, y - 0.2f, 11.0f * S, 2.6f * S,
                    255, 196, 118, (unsigned char)(96 * NightT4() + 26));

    // ---- Awning poles ------------------------------------------------------
    glColor3ub(86, 62, 42);
    glLineWidth(2.4f * S);
    glBegin(GL_LINES);
        glVertex2f(x - 3.9f * S, y);        glVertex2f(x - 3.9f * S, y + 5.0f * S);
        glVertex2f(x + 3.9f * S, y);        glVertex2f(x + 3.9f * S, y + 5.0f * S);
    glEnd();

    // ---- Counter -----------------------------------------------------------
    glColor3ub(128, 88, 54);
    glBegin(GL_QUADS);
        glVertex2f(x - 3.3f * S, y + 0.30f * S);
        glVertex2f(x + 1.5f * S, y + 0.30f * S);
        glVertex2f(x + 1.5f * S, y + 2.15f * S);
        glVertex2f(x - 3.3f * S, y + 2.15f * S);
    glEnd();
    // Plank lines
    glColor4ub(88, 58, 34, 190);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        for (int i = 1; i < 5; i++) {
            float px = x + (-3.3f + i * 0.96f) * S;
            glVertex2f(px, y + 0.30f * S); glVertex2f(px, y + 2.15f * S);
        }
    glEnd();
    // Worktop
    glColor3ub(172, 128, 82);
    glBegin(GL_QUADS);
        glVertex2f(x - 3.5f * S, y + 2.15f * S);
        glVertex2f(x + 1.7f * S, y + 2.15f * S);
        glVertex2f(x + 1.7f * S, y + 2.42f * S);
        glVertex2f(x - 3.5f * S, y + 2.42f * S);
    glEnd();

    // ---- Brass urn, the centrepiece ---------------------------------------
    float ux = x - 2.3f * S, uy = y + 2.42f * S;
    glColor3ub(196, 146, 62);
    glBegin(GL_POLYGON);
        glVertex2f(ux - 0.62f * S, uy);
        glVertex2f(ux + 0.62f * S, uy);
        glVertex2f(ux + 0.70f * S, uy + 0.95f * S);
        glVertex2f(ux + 0.42f * S, uy + 1.62f * S);
        glVertex2f(ux - 0.42f * S, uy + 1.62f * S);
        glVertex2f(ux - 0.70f * S, uy + 0.95f * S);
    glEnd();
    glColor3ub(232, 186, 96);                       // highlight down one side
    glLineWidth(1.6f * S);
    glBegin(GL_LINES);
        glVertex2f(ux - 0.34f * S, uy + 0.18f * S);
        glVertex2f(ux - 0.44f * S, uy + 1.36f * S);
    glEnd();
    glColor3ub(120, 84, 36);                        // tap and lid
    glLineWidth(2.0f * S);
    glBegin(GL_LINES);
        glVertex2f(ux + 0.60f * S, uy + 0.60f * S);
        glVertex2f(ux + 1.00f * S, uy + 0.42f * S);
    glEnd();
    FilledCircle4(ux, uy + 1.74f * S, 0.22f * S, 150, 108, 48, 255);
    // Steam off the urn -- the tallest plume on the stall
    for (int i = 0; i < 5; i++) {
        float t = fmodf(firePhase4 * 0.26f + i * 0.2f, 1.0f);
        FilledCircle4(ux + sinf(t * 3.4f + i) * 0.42f * S,
                      uy + (1.9f + t * 3.0f) * S,
                      (0.16f + t * 0.42f) * S, 236, 240, 246,
                      (unsigned char)(150 * (1.0f - t)));
    }

    // ---- Kettle on a burner ------------------------------------------------
    float kx = x - 0.5f * S, ky = y + 2.42f * S;
    glColor3ub(58, 58, 66);
    glBegin(GL_POLYGON);
        glVertex2f(kx - 0.46f * S, ky);
        glVertex2f(kx + 0.46f * S, ky);
        glVertex2f(kx + 0.38f * S, ky + 0.68f * S);
        glVertex2f(kx - 0.38f * S, ky + 0.68f * S);
    glEnd();
    glColor3ub(40, 40, 48);
    glLineWidth(1.6f * S);
    glBegin(GL_LINE_STRIP);                          // handle
        glVertex2f(kx - 0.30f * S, ky + 0.66f * S);
        glVertex2f(kx,             ky + 1.02f * S);
        glVertex2f(kx + 0.30f * S, ky + 0.66f * S);
    glEnd();
    glBegin(GL_LINES);                               // spout
        glVertex2f(kx + 0.44f * S, ky + 0.42f * S);
        glVertex2f(kx + 0.86f * S, ky + 0.62f * S);
    glEnd();
    // Burner flame under it
    float flick = 0.7f + 0.3f * sinf(firePhase4 * 3.4f);
    glColor4ub(255, 150, 60, (unsigned char)(200 * flick));
    glBegin(GL_TRIANGLES);
        glVertex2f(kx - 0.26f * S, ky - 0.30f * S);
        glVertex2f(kx + 0.26f * S, ky - 0.30f * S);
        glVertex2f(kx, ky + 0.06f * S * flick);
    glEnd();

    // ---- Glasses lined up on the worktop ----------------------------------
    for (int i = 0; i < 5; i++) {
        float gx = x + (0.35f + i * 0.28f) * S;
        glColor3ub(216, 226, 232);
        glBegin(GL_QUADS);
            glVertex2f(gx - 0.09f * S, ky);
            glVertex2f(gx + 0.09f * S, ky);
            glVertex2f(gx + 0.08f * S, ky + 0.34f * S);
            glVertex2f(gx - 0.08f * S, ky + 0.34f * S);
        glEnd();
        glColor3ub(178, 118, 58);                    // tea in the bottom half
        glBegin(GL_QUADS);
            glVertex2f(gx - 0.08f * S, ky + 0.02f * S);
            glVertex2f(gx + 0.08f * S, ky + 0.02f * S);
            glVertex2f(gx + 0.08f * S, ky + 0.18f * S);
            glVertex2f(gx - 0.08f * S, ky + 0.18f * S);
        glEnd();
    }

    // ---- Vendor behind the counter, pouring -------------------------------
    float pour = sinf(pedTimer4 * 0.8f) * 0.10f;
    glColor3ub(74, 96, 120);
    glLineWidth(5.4f * S);
    glBegin(GL_LINES);
        glVertex2f(x + 0.60f * S, y + 2.20f * S);
        glVertex2f(x + 0.52f * S, y + 3.62f * S);
    glEnd();
    glColor3ub(58, 78, 100);
    glLineWidth(2.2f * S);
    glBegin(GL_LINES);                               // arm out over the glasses
        glVertex2f(x + 0.54f * S, y + 3.42f * S);
        glVertex2f(x - 0.10f * S, y + (3.02f + pour) * S);
    glEnd();
    FilledCircle4(x + 0.52f * S, y + 3.96f * S, 0.32f * S, 230, 192, 156, 255);
    FilledCircle4(x + 0.52f * S, y + 4.16f * S, 0.30f * S, 190, 78, 66, 255);

    // ---- Awning ------------------------------------------------------------
    // A sagging canvas sheet rather than a rigid triangle: two curves, so it
    // reads as cloth strung between poles.
    glColor3ub(168, 74, 62);
    glBegin(GL_QUAD_STRIP);
        for (int i = 0; i <= 12; i++) {
            float t  = (float)i / 12.0f;
            float px = x + (-4.4f + 8.8f * t) * S;
            float sag = -sinf(t * PI4) * 0.55f * S;
            glVertex2f(px, y + 5.0f * S + sag);
            glVertex2f(px, y + 4.55f * S + sag);
        }
    glEnd();
    // Scalloped fringe along the front edge
    glColor3ub(196, 96, 80);
    for (int i = 0; i <= 9; i++) {
        float t  = (float)i / 9.0f;
        float px = x + (-4.4f + 8.8f * t) * S;
        float sag = -sinf(t * PI4) * 0.55f * S;
        FilledCircle4(px, y + 4.55f * S + sag, 0.22f * S, 196, 96, 80, 255);
    }
    // Snow lying along the top of the canvas, in winter
    glColor4ub(246, 249, 255, (unsigned char)(235 * SnowDepth4()));
    glLineWidth(3.0f * S);
    glBegin(GL_LINE_STRIP);
        for (int i = 0; i <= 12; i++) {
            float t  = (float)i / 12.0f;
            float px = x + (-4.4f + 8.8f * t) * S;
            float sag = -sinf(t * PI4) * 0.55f * S;
            glVertex2f(px, y + 5.08f * S + sag);
        }
    glEnd();

    // ---- Bulb hanging under the awning ------------------------------------
    float bx = x - 1.0f * S, by = y + 4.30f * S;
    glColor3ub(40, 40, 48);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        glVertex2f(bx, y + 4.75f * S); glVertex2f(bx, by);
    glEnd();
    FilledCircle4(bx, by, 2.30f * S, 255, 206, 132, (unsigned char)(64 * NightT4()));
    FilledCircle4(bx, by, 0.26f * S, 255, 244, 206,
                  (unsigned char)(180 + 75 * NightT4()));

    // ---- Bench, stools and the people on them -----------------------------
    // Bench runs along the front of the counter, facing it.
    glColor3ub(112, 78, 48);
    glBegin(GL_QUADS);
        glVertex2f(x - 4.6f * S, y + 0.86f * S);
        glVertex2f(x - 0.4f * S, y + 0.86f * S);
        glVertex2f(x - 0.4f * S, y + 1.12f * S);
        glVertex2f(x - 4.6f * S, y + 1.12f * S);
    glEnd();
    glColor3ub(84, 56, 34);
    glLineWidth(2.0f * S);
    glBegin(GL_LINES);
        glVertex2f(x - 4.3f * S, y + 0.86f * S); glVertex2f(x - 4.3f * S, y);
        glVertex2f(x - 0.8f * S, y + 0.86f * S); glVertex2f(x - 0.8f * S, y);
    glEnd();

    // Two stools off to the right
    for (int i = 0; i < 2; i++) {
        float sx = x + (2.9f + i * 1.9f) * S;
        glColor3ub(112, 78, 48);
        glBegin(GL_QUADS);
            glVertex2f(sx - 0.46f * S, y + 0.92f * S);
            glVertex2f(sx + 0.46f * S, y + 0.92f * S);
            glVertex2f(sx + 0.46f * S, y + 1.14f * S);
            glVertex2f(sx - 0.46f * S, y + 1.14f * S);
        glEnd();
        glColor3ub(84, 56, 34);
        glLineWidth(1.8f * S);
        glBegin(GL_LINES);
            glVertex2f(sx - 0.34f * S, y + 0.92f * S); glVertex2f(sx - 0.34f * S, y);
            glVertex2f(sx + 0.34f * S, y + 0.92f * S); glVertex2f(sx + 0.34f * S, y);
        glEnd();
    }

    // Five customers: three along the bench facing the counter, two on the
    // stools turned the other way.
    DrawTeaSitter4(x - 3.9f * S, y, S * 0.92f, 0, 148,  58,  64, false);
    DrawTeaSitter4(x - 2.7f * S, y, S * 0.96f, 1,  56,  84, 128, false);
    DrawTeaSitter4(x - 1.5f * S, y, S * 0.90f, 2,  92, 112,  70, false);
    DrawTeaSitter4(x + 2.9f * S, y, S * 0.94f, 3, 132,  96,  52, true );
    DrawTeaSitter4(x + 4.8f * S, y, S * 0.88f, 4,  86,  70, 116, true );

    glLineWidth(1.0f);
}

// ---- Car ------------------------------------------------------------------
//  Same trick as the rickshaw: position and size come straight from the road
//  functions, so it keeps to the carriageway and grows at the road's own rate.
//  It runs in the LEFT-hand wheel track.
//
//  FRONT VIEW. It travels from the far end of the street down toward the
//  camera and the river, so what the viewer is looking at is its nose, not its
//  flank -- grille, bonnet, windscreen, two headlamps and two front wheels
//  set either side. A side profile on this path read as a car sliding
//  sideways across the street; head-on it reads as coming down it.
//
//  Nothing here is hand-placed against the road. Every coordinate is the
//  centreline x, the road's y, and the road-derived scale S, so the car still
//  tracks the curve and grows exactly as the carriageway does.
float carT4      = 0.62f;
float carWheel4  = 0.0f;

void DrawCar4() {
    const float t  = carT4;
    const float y  = RoadY4(t);
    const float cx = RoadCX4(t);
    const float S  = 0.30f + 1.25f * powf(t, 1.28f);
    const float x  = cx - RoadHW4(t) * 0.36f;        // left-hand track
    const float depth = 0.80f * (1.0f - t);

    // The street bends left as it comes forward. Sampling the centreline a
    // little further down the road gives the direction the car is pointing,
    // and shearing the upper bodywork by that amount leans it into the bend
    // instead of leaving it square to the camera on a curving road.
    const float ahead = fminf(t + 0.07f, 1.0f);
    const float lean  = (RoadCX4(ahead) - cx) * 0.05f;   // x shift per unit height
    #define CX4(hx, hy) ((hx) + lean * (hy))             // hy in S-units above the road

    unsigned char r, g, b;
    DrawGroundShadow(x, y, 2.3f * S, 0.34f, (unsigned char)(95 * (0.35f + 0.65f * t)));

    // ---- Wheels -----------------------------------------------------------
    // Head-on a wheel is the tread face: a tall narrow ellipse, not a disc.
    // Drawn before the body so the arches crop them.
    for (int w = 0; w < 2; w++) {
        float wx = (w == 0) ? (x - 1.72f * S) : (x + 1.72f * S);
        float wy = y + 0.74f * S;
        float rx = 0.32f * S, ry = 0.78f * S;

        AirFade4(depth, y, 16, 18, 24, 44, 48, 58, r, g, b);
        glColor3ub(r, g, b);
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(wx, wy);
            for (int k = 0; k <= 16; k++) {
                float a = (float)k / 16.0f * 2.0f * PI4;
                glVertex2f(wx + rx * cosf(a), wy + ry * sinf(a));
            }
        glEnd();
        // Hub, and a couple of spokes so the wheel still reads as turning.
        AirFade4(depth, y, 76, 80, 90, 142, 146, 158, r, g, b);
        glColor3ub(r, g, b);
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(wx, wy);
            for (int k = 0; k <= 12; k++) {
                float a = (float)k / 12.0f * 2.0f * PI4;
                glVertex2f(wx + rx * 0.58f * cosf(a), wy + ry * 0.58f * sinf(a));
            }
        glEnd();
        AirFade4(depth, y, 38, 40, 48, 80, 84, 96, r, g, b);
        glColor3ub(r, g, b);
        glLineWidth(1.0f);
        glBegin(GL_LINES);
            for (int k = 0; k < 3; k++) {
                float a = carWheel4 + k * (PI4 / 3.0f);
                glVertex2f(wx - rx * 0.52f * cosf(a), wy - ry * 0.52f * sinf(a));
                glVertex2f(wx + rx * 0.52f * cosf(a), wy + ry * 0.52f * sinf(a));
            }
        glEnd();
    }

    // ---- Body, seen nose-on -----------------------------------------------
    // A car is far narrower than it is long, so the front is a squat block
    // roughly two thirds the width the old side view was long.
    const float HWB = 1.86f * S;         // half-width at the waist
    AirFade4(depth, y, 96, 42, 48, 178, 74, 78, r, g, b);
    glColor3ub(r, g, b);
    glBegin(GL_POLYGON);
        glVertex2f(CX4(x - HWB * 0.94f, 0.42f), y + 0.42f * S);   // valance
        glVertex2f(CX4(x + HWB * 0.94f, 0.42f), y + 0.42f * S);
        glVertex2f(CX4(x + HWB,         1.10f), y + 1.10f * S);
        glVertex2f(CX4(x + HWB,         1.78f), y + 1.78f * S);   // shoulder
        glVertex2f(CX4(x - HWB,         1.78f), y + 1.78f * S);
        glVertex2f(CX4(x - HWB,         1.10f), y + 1.10f * S);
    glEnd();

    // Bonnet: the top surface of the nose, catching the sky, so a shade
    // lighter than the front panel below it.
    AirFade4(depth, y, 116, 54, 60, 202, 92, 96, r, g, b);
    glColor3ub(r, g, b);
    glBegin(GL_POLYGON);
        glVertex2f(CX4(x - HWB,         1.78f), y + 1.78f * S);
        glVertex2f(CX4(x + HWB,         1.78f), y + 1.78f * S);
        glVertex2f(CX4(x + HWB * 0.90f, 2.06f), y + 2.06f * S);
        glVertex2f(CX4(x - HWB * 0.90f, 2.06f), y + 2.06f * S);
    glEnd();

    // ---- Cabin ------------------------------------------------------------
    // Narrower than the body and set back, with the roof narrower again.
    const float HWC = 1.52f * S;
    AirFade4(depth, y, 86, 36, 42, 162, 66, 70, r, g, b);
    glColor3ub(r, g, b);
    glBegin(GL_POLYGON);
        glVertex2f(CX4(x - HWC,         2.06f), y + 2.06f * S);
        glVertex2f(CX4(x + HWC,         2.06f), y + 2.06f * S);
        glVertex2f(CX4(x + HWC * 0.88f, 3.24f), y + 3.24f * S);
        glVertex2f(CX4(x - HWC * 0.88f, 3.24f), y + 3.24f * S);
    glEnd();

    // Windscreen
    AirFade4(depth, y, 38, 52, 70, 132, 158, 184, r, g, b);
    glColor3ub(r, g, b);
    glBegin(GL_POLYGON);
        glVertex2f(CX4(x - HWC * 0.86f, 2.22f), y + 2.22f * S);
        glVertex2f(CX4(x + HWC * 0.86f, 2.22f), y + 2.22f * S);
        glVertex2f(CX4(x + HWC * 0.76f, 3.10f), y + 3.10f * S);
        glVertex2f(CX4(x - HWC * 0.76f, 3.10f), y + 3.10f * S);
    glEnd();
    // A-pillars, so the glass is framed rather than floating
    AirFade4(depth, y, 70, 28, 34, 138, 56, 60, r, g, b);
    glColor3ub(r, g, b);
    glLineWidth(1.6f * S);
    glBegin(GL_LINES);
        glVertex2f(CX4(x - HWC, 2.06f), y + 2.06f * S);
        glVertex2f(CX4(x - HWC * 0.88f, 3.24f), y + 3.24f * S);
        glVertex2f(CX4(x + HWC, 2.06f), y + 2.06f * S);
        glVertex2f(CX4(x + HWC * 0.88f, 3.24f), y + 3.24f * S);
    glEnd();

    // Driver and passenger behind the glass, and the wiper sweep below them
    AirFade4(depth, y, 26, 26, 34, 58, 58, 70, r, g, b);
    FilledCircle4(CX4(x - 0.66f * S, 2.72f), y + 2.72f * S, 0.28f * S, r, g, b, 235);
    FilledCircle4(CX4(x + 0.70f * S, 2.70f), y + 2.70f * S, 0.25f * S, r, g, b, 200);
    AirFade4(depth, y, 44, 46, 54, 96, 100, 112, r, g, b);
    glColor3ub(r, g, b);
    glLineWidth(1.1f * S);
    glBegin(GL_LINES);
        glVertex2f(CX4(x - 1.06f * S, 2.24f), y + 2.24f * S);
        glVertex2f(CX4(x - 0.24f * S, 2.62f), y + 2.62f * S);
        glVertex2f(CX4(x + 0.30f * S, 2.24f), y + 2.24f * S);
        glVertex2f(CX4(x + 1.08f * S, 2.62f), y + 2.62f * S);
    glEnd();

    // ---- Grille and bumper ------------------------------------------------
    AirFade4(depth, y, 22, 22, 28, 52, 54, 62, r, g, b);
    glColor3ub(r, g, b);
    glBegin(GL_QUADS);
        glVertex2f(CX4(x - 0.96f * S, 1.16f), y + 1.16f * S);
        glVertex2f(CX4(x + 0.96f * S, 1.16f), y + 1.16f * S);
        glVertex2f(CX4(x + 0.96f * S, 1.66f), y + 1.66f * S);
        glVertex2f(CX4(x - 0.96f * S, 1.66f), y + 1.66f * S);
    glEnd();
    AirFade4(depth, y, 118, 122, 132, 186, 190, 198, r, g, b);   // grille bars
    glColor3ub(r, g, b);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        for (int k = 0; k < 3; k++) {
            float gy = 1.26f + k * 0.16f;
            glVertex2f(CX4(x - 0.92f * S, gy), y + gy * S);
            glVertex2f(CX4(x + 0.92f * S, gy), y + gy * S);
        }
    glEnd();
    // Bumper across the bottom, and a number plate on it
    AirFade4(depth, y, 40, 42, 50, 88, 92, 102, r, g, b);
    glColor3ub(r, g, b);
    glBegin(GL_QUADS);
        glVertex2f(CX4(x - HWB * 0.92f, 0.60f), y + 0.60f * S);
        glVertex2f(CX4(x + HWB * 0.92f, 0.60f), y + 0.60f * S);
        glVertex2f(CX4(x + HWB * 0.92f, 1.02f), y + 1.02f * S);
        glVertex2f(CX4(x - HWB * 0.92f, 1.02f), y + 1.02f * S);
    glEnd();
    glColor4ub(228, 232, 236, 230);
    glBegin(GL_QUADS);
        glVertex2f(CX4(x - 0.46f * S, 0.68f), y + 0.68f * S);
        glVertex2f(CX4(x + 0.46f * S, 0.68f), y + 0.68f * S);
        glVertex2f(CX4(x + 0.46f * S, 0.94f), y + 0.94f * S);
        glVertex2f(CX4(x - 0.46f * S, 0.94f), y + 0.94f * S);
    glEnd();

    // Snow on the roof, in winter
    glColor4ub(246, 249, 255, (unsigned char)(235 * SnowDepth4()));
    glBegin(GL_QUADS);
        glVertex2f(CX4(x - HWC * 0.88f, 3.24f), y + 3.24f * S);
        glVertex2f(CX4(x + HWC * 0.88f, 3.24f), y + 3.24f * S);
        glVertex2f(CX4(x + HWC * 0.84f, 3.44f), y + 3.44f * S);
        glVertex2f(CX4(x - HWC * 0.84f, 3.44f), y + 3.44f * S);
    glEnd();

    // ---- Headlamps --------------------------------------------------------
    // Both of them face the viewer now, so at night this is the car's whole
    // signature: two lamps and the wash they throw on the road in front.
    const float lampOn = NightT4();
    const float hlY = y + 1.62f * S;
    const float hlX = 1.40f * S;
    for (int k = 0; k < 2; k++) {
        float lx = CX4(x + (k ? hlX : -hlX), 1.62f);
        // Lens is visible even by day
        FilledCircle4(lx, hlY, 0.30f * S, 226, 228, 224, 235);
        FilledCircle4(lx, hlY, 0.19f * S, 252, 248, 232, 255);
        if (lampOn > 0.05f) {
            FilledCircle4(lx, hlY, 0.78f * S, 255, 240, 200, (unsigned char)(90 * lampOn));
            FilledCircle4(lx, hlY, 0.30f * S, 255, 252, 238, (unsigned char)(255 * lampOn));
        }
    }
    if (lampOn > 0.05f) {
        // A pair of beams spilling down onto the road between the viewer and
        // the car, widening as they come forward.
        for (int k = 0; k < 2; k++) {
            float dir = k ? 1.0f : -1.0f;
            DrawSoftEllipse(x + dir * 1.9f * S, y - 1.5f * S, 2.9f * S, 1.1f * S,
                            255, 236, 190, (unsigned char)(95 * lampOn), 3);
        }
        DrawSoftEllipse(x, y - 2.6f * S, 5.2f * S, 1.5f * S,
                        255, 236, 190, (unsigned char)(78 * lampOn), 4);
    }

    // Exhaust vapour, drifting out from behind the car rather than off one end
    for (int i = 0; i < 4; i++) {
        float vt = fmodf(firePhase4 * 0.30f + i * 0.25f, 1.0f);
        float dir = (i & 1) ? 1.0f : -1.0f;
        FilledCircle4(x + dir * (1.4f + vt * 1.3f) * S, y + (1.9f + vt * 0.9f) * S,
                      (0.18f + vt * 0.42f) * S, 228, 234, 242,
                      (unsigned char)(105 * (1.0f - vt) * SeasonWinter4()));
    }
    glLineWidth(1.0f);
    #undef CX4
}

void UpdateCar4(int) {
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateCar4, 0); return; }
    if (isAnimating4) {
        float step = 0.0026f / (0.35f + 1.15f * carT4);
        carT4 += step;
        carWheel4 += step * (30.0f + 52.0f * carT4);
        if (carT4 > 1.16f) carT4 = -0.08f;
    }
    glutTimerFunc(30, UpdateCar4, 0);
}

// ---- Bicycle --------------------------------------------------------------
//  The slowest thing on the street, keeping to the right-hand edge where a
//  cyclist actually rides.
//
//  FRONT VIEW, for the same reason as the car: it is riding down the street
//  toward the camera and the river, so the viewer sees the front wheel edge-on,
//  the handlebars spread across, and the rider facing them. The rear wheel is
//  directly behind the front one and shows only as a sliver, which is exactly
//  what makes the bike read as coming at you rather than crossing.
float bikeT4     = 0.34f;
float bikeWheel4 = 0.0f;

void DrawBike4() {
    const float t  = bikeT4;
    const float y  = RoadY4(t);
    const float cx = RoadCX4(t);
    const float S  = 0.30f + 1.25f * powf(t, 1.28f);
    const float x  = cx + RoadHW4(t) * 0.72f;        // hugging the right kerb
    const float depth = 0.80f * (1.0f - t);

    unsigned char r, g, b;
    DrawGroundShadow(x, y, 1.1f * S, 0.30f, (unsigned char)(85 * (0.35f + 0.65f * t)));

    const float wr = 0.80f * S;          // wheel radius (its tall axis)
    const float rx = 0.15f * S;          // and its width, seen edge-on
    const float wy = y + wr;

    // A rider out of the saddle rocks the bike under them. One sway value
    // drives the bars, the frame and the torso together so the whole machine
    // leans as a unit rather than coming apart.
    const float sway = sinf(bikeWheel4) * 0.10f * S;

    // ---- Rear wheel, directly behind and mostly hidden --------------------
    AirFade4(depth, y, 18, 20, 26, 46, 50, 60, r, g, b);
    glColor3ub(r, g, b);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(x - sway * 0.5f, wy + 0.06f * S);
        for (int k = 0; k <= 14; k++) {
            float a = (float)k / 14.0f * 2.0f * PI4;
            glVertex2f(x - sway * 0.5f + rx * 1.55f * cosf(a),
                       wy + 0.06f * S  + wr * 0.94f * sinf(a));
        }
    glEnd();

    // ---- Front wheel, edge-on ---------------------------------------------
    AirFade4(depth, y, 26, 28, 34, 60, 64, 74, r, g, b);
    glColor3ub(r, g, b);
    glLineWidth(2.2f * S);
    glBegin(GL_LINE_LOOP);
        for (int k = 0; k < 18; k++) {
            float a = (float)k / 18.0f * 2.0f * PI4;
            glVertex2f(x + sway + rx * cosf(a), wy + wr * sinf(a));
        }
    glEnd();
    // Spokes, squashed onto the same narrow ellipse so they turn with it
    AirFade4(depth, y, 88, 92, 104, 150, 154, 166, r, g, b);
    glColor3ub(r, g, b);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        for (int k = 0; k < 4; k++) {
            float a = bikeWheel4 + k * (PI4 / 4.0f);
            glVertex2f(x + sway - rx * 0.86f * cosf(a), wy - wr * 0.86f * sinf(a));
            glVertex2f(x + sway + rx * 0.86f * cosf(a), wy + wr * 0.86f * sinf(a));
        }
    glEnd();

    // ---- Frame: fork, head tube and bars ----------------------------------
    AirFade4(depth, y, 42, 88, 118, 84, 156, 196, r, g, b);
    glColor3ub(r, g, b);
    glLineWidth(1.7f * S);
    glBegin(GL_LINES);
        glVertex2f(x + sway - 0.20f * S, wy);                 // fork legs,
        glVertex2f(x + sway - 0.05f * S, y + 2.16f * S);      // splayed at the
        glVertex2f(x + sway + 0.20f * S, wy);                 // hub and closing
        glVertex2f(x + sway + 0.05f * S, y + 2.16f * S);      // at the crown
        glVertex2f(x + sway, y + 2.16f * S);                  // stem
        glVertex2f(x + sway, y + 2.40f * S);
        glVertex2f(x - 0.10f * S, y + 1.05f * S);             // down tube back
        glVertex2f(x + sway, y + 2.10f * S);                  // to the head
    glEnd();
    // Handlebars, spread across the frame -- the giveaway that this is a
    // bicycle seen head-on.
    AirFade4(depth, y, 30, 30, 36, 62, 62, 72, r, g, b);
    glColor3ub(r, g, b);
    glLineWidth(2.4f * S);
    glBegin(GL_LINE_STRIP);
        glVertex2f(x + sway - 0.66f * S, y + 2.30f * S);
        glVertex2f(x + sway - 0.20f * S, y + 2.42f * S);
        glVertex2f(x + sway + 0.20f * S, y + 2.42f * S);
        glVertex2f(x + sway + 0.66f * S, y + 2.30f * S);
    glEnd();

    // ---- Rider ------------------------------------------------------------
    // Legs alternate with the crank. Seen from the front the knees swing out
    // to the sides rather than back and forth, so the drive is in x as much
    // as in y.
    const float pedA = sinf(bikeWheel4), pedB = sinf(bikeWheel4 + PI4);
    const float hipY = y + 2.58f * S, shoY = y + 3.62f * S;
    const float hx   = sway * 0.6f;

    AirFade4(depth, y, 44, 44, 54, 82, 82, 96, r, g, b);
    glColor3ub(r, g, b);
    glLineWidth(1.9f * S);
    glBegin(GL_LINE_STRIP);                                   // left leg
        glVertex2f(x + hx - 0.22f * S, hipY);
        glVertex2f(x + hx - 0.52f * S, y + 1.62f * S + 0.14f * S * pedA);
        glVertex2f(x + hx - 0.34f * S, y + 0.62f * S + 0.26f * S * pedA);
    glEnd();
    glBegin(GL_LINE_STRIP);                                   // right leg
        glVertex2f(x + hx + 0.22f * S, hipY);
        glVertex2f(x + hx + 0.52f * S, y + 1.62f * S + 0.14f * S * pedB);
        glVertex2f(x + hx + 0.34f * S, y + 0.62f * S + 0.26f * S * pedB);
    glEnd();

    // Torso, squared to the viewer: widest at the shoulders.
    AirFade4(depth, y, 116, 52, 60, 196, 92, 98, r, g, b);   // jacket
    glColor3ub(r, g, b);
    glBegin(GL_POLYGON);
        glVertex2f(x + hx - 0.32f * S, hipY);
        glVertex2f(x + hx + 0.32f * S, hipY);
        glVertex2f(x + hx + 0.44f * S, shoY);
        glVertex2f(x + hx - 0.44f * S, shoY);
    glEnd();
    // Arms out and down to the grips
    glLineWidth(1.7f * S);
    glBegin(GL_LINES);
        glVertex2f(x + hx - 0.42f * S, shoY - 0.10f * S);
        glVertex2f(x + sway - 0.62f * S, y + 2.34f * S);
        glVertex2f(x + hx + 0.42f * S, shoY - 0.10f * S);
        glVertex2f(x + sway + 0.62f * S, y + 2.34f * S);
    glEnd();
    AirFade4(depth, y, 150, 122, 100, 226, 188, 152, r, g, b);
    FilledCircle4(x + hx, shoY + 0.30f * S, 0.27f * S, r, g, b, 255);   // face
    AirFade4(depth, y, 190, 168, 60, 236, 214, 92, r, g, b);            // helmet
    FilledCircle4(x + hx, shoY + 0.46f * S, 0.28f * S, r, g, b, 255);

    // ---- Front lamp -------------------------------------------------------
    // Pointing straight at the viewer now, so it is a disc rather than a beam
    // raking off to one side.
    float lampOn = NightT4();
    if (lampOn > 0.05f) {
        // On the fork crown, below the bars: a halo up at bar height washed
        // the whole machine out at the far end of the street, where the bike
        // is only a few pixels tall.
        float lx = x + sway, ly = y + 1.78f * S;
        FilledCircle4(lx, ly, 0.34f * S, 255, 232, 176, (unsigned char)(80 * lampOn));
        FilledCircle4(lx, ly, 0.15f * S, 255, 250, 226, (unsigned char)(255 * lampOn));
        DrawSoftEllipse(lx, y - 1.5f * S, 3.0f * S, 1.0f * S,
                        255, 232, 176, (unsigned char)(72 * lampOn), 3);
    }
    glLineWidth(1.0f);
}

void UpdateBike4(int) {
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateBike4, 0); return; }
    if (isAnimating4) {
        float step = 0.0011f / (0.35f + 1.15f * bikeT4);
        bikeT4 += step;
        bikeWheel4 += step * (22.0f + 40.0f * bikeT4);
        if (bikeT4 > 1.14f) bikeT4 = -0.05f;
    }
    glutTimerFunc(30, UpdateBike4, 0);
}

// The rickshaw is parked, so this no longer advances it: rickshawT4 stays at
// RICKSHAW_PARK_T4 and the wheels stay still. The timer itself is kept so the
// call in InitScenario4() and the depth-sort entry both remain valid.
void UpdateRickshaw4(int) {
    rickshawT4     = RICKSHAW_PARK_T4;
    rickshawWheel4 = 0.0f;
    glutTimerFunc(120, UpdateRickshaw4, 0);
}

void DrawBuildingShadows4() {
    unsigned char sr = MixB4(10, 92), sg = MixB4(14, 104), sb = MixB4(28, 126);
    unsigned char a  = (unsigned char)(MixB4(96, 70));

    for (int i = 0; i < NUM_SHOPS4; i++) {
        const Shop4& sh = shops4[i];
        float half = sh.w * 0.5f + SHOP_EAVE_OVER4;
        // Length scales with height, and leans left, away from the light.
        float len  = 1.1f + sh.h * 0.055f;
        glBegin(GL_QUADS);
            glColor4ub(sr, sg, sb, a);
            glVertex2f(sh.x - half,          SHOP_BASE_Y4);
            glVertex2f(sh.x + half,          SHOP_BASE_Y4);
            glColor4ub(sr, sg, sb, 0);
            glVertex2f(sh.x + half - len * 0.8f, SHOP_BASE_Y4 - len);
            glVertex2f(sh.x - half - len * 1.5f, SHOP_BASE_Y4 - len);
        glEnd();
    }

    // The street canyon lays its own shadow into the mouth of the road.
    glBegin(GL_QUADS);
        glColor4ub(sr, sg, sb, (unsigned char)(a * 0.9f));
        glVertex2f(-49.0f, SHOP_BASE_Y4);
        glVertex2f(-28.0f, SHOP_BASE_Y4);
        glColor4ub(sr, sg, sb, 0);
        glVertex2f(-29.5f, SHOP_BASE_Y4 - 2.2f);
        glVertex2f(-51.5f, SHOP_BASE_Y4 - 2.2f);
    glEnd();

    // A cool ambient occlusion band right where the row meets the snow, so
    // nothing in the back line appears to hover.
    glBegin(GL_QUADS);
        glColor4ub(MixB4(16, 120), MixB4(22, 132), MixB4(40, 152), (unsigned char)(MixB4(70, 44)));
        glVertex2f(-60.0f, SHOP_BASE_Y4);
        glVertex2f( 60.0f, SHOP_BASE_Y4);
        glColor4ub(MixB4(16, 120), MixB4(22, 132), MixB4(40, 152), 0);
        glVertex2f( 60.0f, SHOP_BASE_Y4 - 1.4f);
        glVertex2f(-60.0f, SHOP_BASE_Y4 - 1.4f);
    glEnd();
}

void DrawPerson4(const Ped4& p, float t) {
    float sc = DepthScaleRange(p.y, -10.5f, -14.0f, 0.88f, 1.12f);
    DrawGroundShadow(p.x, p.y, 0.6f * sc, 0.25f, 78);
    BeginDepthSprite(p.x, p.y, sc);

    float bob = sinf(t*3.0f + p.phase) * 0.1f;
    float hipX = p.x, hipY = p.y + 1.0f + bob;
    glColor3ub(30, 30, 35);
    glLineWidth(2.5f);
    glBegin(GL_LINES);
        glVertex2f(hipX, hipY); glVertex2f(hipX + 0.22f*sinf(t*4.0f+p.phase), p.y);
        glVertex2f(hipX, hipY); glVertex2f(hipX - 0.22f*sinf(t*4.0f+p.phase), p.y);
    glEnd();
    glColor3ub(p.coatR, p.coatG, p.coatB);
    glLineWidth(5.0f);
    glBegin(GL_LINES); glVertex2f(hipX, hipY); glVertex2f(hipX, hipY+1.15f); glEnd();
    FilledCircle4(hipX, hipY+1.45f, 0.3f, 225, 185, 145, 255);
    glColor3ub(40, 40, 45);
    glBegin(GL_TRIANGLES);
        glVertex2f(hipX-0.3f, hipY+1.65f); glVertex2f(hipX+0.3f, hipY+1.65f); glVertex2f(hipX, hipY+2.2f);
    glEnd();
    // A little snow settles on the hat
    glColor4ub(246, 249, 255, 230);
    glBegin(GL_TRIANGLES);
        glVertex2f(hipX-0.16f*SnowDepth4(), hipY+1.95f);
        glVertex2f(hipX+0.16f*SnowDepth4(), hipY+1.95f);
        glVertex2f(hipX, hipY+2.2f);
    glEnd();

    // Breath. The horse already had this and it was the single best detail
    // in the scenario -- it costs two lines to give it to everyone, and it
    // is the cheapest possible way to say "cold".
    for (int i = 0; i < 3; i++) {
        float bt = fmodf(firePhase4 * 0.26f + p.phase * 0.17f + i * 0.33f, 1.0f);
        FilledCircle4(hipX + p.dir * (0.35f + bt * 1.3f), hipY + 1.42f + bt * 0.45f,
                      0.09f + bt * 0.22f, 226, 234, 244,
                      (unsigned char)(105 * (1.0f - bt)));
    }

    EndDepthSprite();
}

void DrawDogPlay4(float x, float y, float scale, float t) {
    // Small dog on the main plaza, right side. It stays out of the river and
    // the near footpath and is deliberately sized to sit at roughly 60% of a
    
    float sc = scale;
    float bodyX = x;
    float bodyY = y;
    float playPhase = t * 0.55f;

    DrawGroundShadow(bodyX, bodyY, 0.72f * sc, 0.18f, 68);
    BeginDepthSprite(bodyX, bodyY, sc);

    // Dog stays in the same place but shifts its body slightly to keep the
    // play animation alive while it watches and nudges a nearby ball.
    float lean = sinf(playPhase) * 8.0f;
    glPushMatrix();
        glTranslatef(bodyX, bodyY, 0.0f);
        glRotatef(lean, 0.0f, 0.0f, 1.0f);
        glTranslatef(-bodyX, -bodyY, 0.0f);

        glColor3ub(118, 90, 58);
        glBegin(GL_POLYGON);
            glVertex2f(bodyX - 0.80f, bodyY + 0.22f);
            glVertex2f(bodyX - 0.20f, bodyY + 0.70f);
            glVertex2f(bodyX + 0.70f, bodyY + 0.62f);
            glVertex2f(bodyX + 0.92f, bodyY + 0.18f);
            glVertex2f(bodyX + 0.72f, bodyY - 0.28f);
            glVertex2f(bodyX - 0.60f, bodyY - 0.30f);
            glVertex2f(bodyX - 0.88f, bodyY - 0.02f);
        glEnd();

        // Legs planted with a soft trot.
        glColor3ub(78, 60, 42);
        glLineWidth(2.2f);
        glBegin(GL_LINES);
            glVertex2f(bodyX - 0.38f, bodyY + 0.10f); glVertex2f(bodyX - 0.44f, bodyY - 0.52f);
            glVertex2f(bodyX + 0.02f,  bodyY + 0.08f); glVertex2f(bodyX + 0.00f,  bodyY - 0.54f);
            glVertex2f(bodyX + 0.40f, bodyY + 0.00f); glVertex2f(bodyX + 0.42f, bodyY - 0.52f);
        glEnd();

        // Head and snout.
        FilledCircle4(bodyX + 0.90f, bodyY + 0.42f, 0.28f, 132, 102, 66, 255);
        glColor3ub(220, 182, 140);
        glBegin(GL_POLYGON);
            glVertex2f(bodyX + 1.08f, bodyY + 0.34f);
            glVertex2f(bodyX + 1.38f, bodyY + 0.40f);
            glVertex2f(bodyX + 1.18f, bodyY + 0.16f);
            glVertex2f(bodyX + 1.02f, bodyY + 0.18f);
        glEnd();

        // Ears and eyes.
        glColor3ub(90, 68, 42);
        glBegin(GL_TRIANGLES);
            glVertex2f(bodyX + 0.95f, bodyY + 0.70f); glVertex2f(bodyX + 1.02f, bodyY + 0.86f); glVertex2f(bodyX + 1.12f, bodyY + 0.64f);
            glVertex2f(bodyX + 1.15f, bodyY + 0.76f); glVertex2f(bodyX + 1.20f, bodyY + 0.90f); glVertex2f(bodyX + 1.30f, bodyY + 0.68f);
        glEnd();
        glColor3ub(32, 32, 32);
        FilledCircle4(bodyX + 0.98f, bodyY + 0.46f, 0.05f, 20, 20, 20, 255);
        FilledCircle4(bodyX + 1.12f, bodyY + 0.46f, 0.05f, 20, 20, 20, 255);

        // Tail wagging in a happy little arc.
        glColor3ub(90, 68, 42);
        glLineWidth(2.0f);
        glBegin(GL_LINES);
            glVertex2f(bodyX - 0.92f, bodyY + 0.18f);
            glVertex2f(bodyX - 1.28f + 0.18f * sinf(t * 2.4f), bodyY + 0.56f + 0.20f * cosf(t * 2.4f));
        glEnd();
    glPopMatrix();

    // Ball sits close by, with only a tiny wobble to suggest play without
    // moving the dog itself across the plaza.
    float bx = bodyX + 1.1f + 0.18f * sinf(playPhase * 1.7f);
    float by = bodyY + 1.05f + 0.10f * cosf(playPhase * 2.3f);
    glColor3ub(220, 90, 70);
    FilledCircle4(bx, by, 0.18f, 220, 90, 70, 255);
    glColor3ub(245, 200, 180);
    glBegin(GL_LINES);
        for (int i = 0; i < 6; i++) {
            float a = playPhase + i * (PI4 / 3.0f);
            glVertex2f(bx, by);
            glVertex2f(bx + 0.12f * cosf(a), by + 0.12f * sinf(a));
        }
    glEnd();

    EndDepthSprite();
}

void DrawPedestrians4() {
    // Nearest walker drawn last, so the crowd overlaps correctly.
    int order[NUM_PEDS4];
    for (int i = 0; i < NUM_PEDS4; i++) order[i] = i;
    for (int i = 1; i < NUM_PEDS4; i++) {
        int key = order[i], j = i - 1;
        while (j >= 0 && peds4[order[j]].y < peds4[key].y) { order[j+1] = order[j]; j--; }
        order[j+1] = key;
    }
    for (int i = 0; i < NUM_PEDS4; i++) DrawPerson4(peds4[order[i]], pedTimer4);
}

void UpdatePedestrians4(int) {
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdatePedestrians4, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating4) {
        pedTimer4 += 0.12f;
        for (int i = 0; i < NUM_PEDS4; i++) {
            peds4[i].x += peds4[i].speed * peds4[i].dir;
            if (peds4[i].dir > 0 && peds4[i].x > 65.0f)  peds4[i].x = -65.0f;
            if (peds4[i].dir < 0 && peds4[i].x < -65.0f) peds4[i].x = 65.0f;
        }
    }
    glutTimerFunc(30, UpdatePedestrians4, 0);
}

// ============================================================================
//  SNOW
// ============================================================================
// ============================================================================
//  SNOW IN THREE DEPTH BANDS
// ----------------------------------------------------------------------------
//  Snowfall used to be one flat layer: every flake the same size, the same
//  brightness and the same speed, all drawn in front of everything else. Now
//  the array is split into three bands drawn at three different points in the
//  frame, which costs nothing and gives the whole scene depth for free.
//
//      FAR  drawn right after the sky   small, dim, slow  -- behind the shops
//      MID  drawn after the props       as before
//      NEAR drawn last of all           large, bright, fast, slightly blurred
// ============================================================================
constexpr int SNOW_FAR_END4  = 150;
constexpr int SNOW_MID_END4  = 250;   // NEAR runs from here to MAX_SNOW4

void InitSnow4() {
    for (int i = 0; i < MAX_SNOW4; i++) {
        snowX4[i] = -62.0f + (rand()%1240)/10.0f;
        snowY4[i] = (rand()%800-400)/10.0f + 20.0f;
        snowDrift4[i] = ((rand()%100)-50)/500.0f;
        snowSize4[i] = 0.15f + (rand()%15)/100.0f;
    }
}

// How many flakes of a band are active at the current intensity.
inline int SnowBandCount4(int first, int last) {
    int n = last - first;
    if (snowIntensity4 == 0) return first + n / 3;
    if (snowIntensity4 == 1) return first + (n * 2) / 3;
    return last;
}

// The same particle arrays carry both seasons: in winter each one is a snow
// flake, in autumn the identical particle is drawn as a tumbling leaf. Doing
// it this way means the depth banding, the fall speeds and the density keys
// all keep working unchanged, and the crossfade dissolves one into the other
// rather than swapping two separate systems.
void DrawSnowBand4(int first, int last, float sizeMul,
                   unsigned char alpha, bool blurred) {
    int end = SnowBandCount4(first, last);
    const float winter = SeasonWinter4();
    const float autumnV = SeasonAutumn4();

    const unsigned char leafCols[4][3] = {
        {186,  92,  38}, {214, 142,  48}, {154,  70,  44}, {198, 168,  62}
    };

    for (int i = first; i < end; i++) {
        float r = snowSize4[i] * sizeMul;

        // ---- Snow ---------------------------------------------------------
        if (winter > 0.02f) {
            unsigned char a = (unsigned char)(alpha * winter);
            if (blurred) {
                // Near flakes are out of focus: a soft halo plus an offset core.
                FilledCircle4(snowX4[i], snowY4[i], r * 1.9f, 255, 255, 255, (unsigned char)(a / 4));
                FilledCircle4(snowX4[i] + r * 0.3f, snowY4[i] - r * 0.2f, r, 255, 255, 255, a);
            } else {
                FilledCircle4(snowX4[i], snowY4[i], r, 255, 255, 255, a);
            }
        }

        // ---- Leaf ---------------------------------------------------------
        if (autumnV > 0.02f) {
            unsigned char a = (unsigned char)(alpha * autumnV);
            const unsigned char* c = leafCols[i % 4];
            // A leaf spins as it falls, and it is wider than a flake.
            float spin = snowY4[i] * 0.55f + i;
            float lr   = r * 2.3f;
            glColor4ub(c[0], c[1], c[2], a);
            glBegin(GL_TRIANGLES);
                glVertex2f(snowX4[i] + lr * cosf(spin),
                           snowY4[i] + lr * 0.55f * sinf(spin));
                glVertex2f(snowX4[i] + lr * cosf(spin + 2.2f),
                           snowY4[i] + lr * 0.55f * sinf(spin + 2.2f));
                glVertex2f(snowX4[i] + lr * cosf(spin + 4.2f),
                           snowY4[i] + lr * 0.55f * sinf(spin + 4.2f));
            glEnd();
            // Midrib, which is what stops it reading as a coloured blob
            glColor4ub((unsigned char)(c[0] * 0.6f), (unsigned char)(c[1] * 0.6f),
                       (unsigned char)(c[2] * 0.6f), a);
            glLineWidth(1.0f);
            glBegin(GL_LINES);
                glVertex2f(snowX4[i] + lr * cosf(spin),
                           snowY4[i] + lr * 0.55f * sinf(spin));
                glVertex2f(snowX4[i] + lr * cosf(spin + 3.2f) * 0.5f,
                           snowY4[i] + lr * 0.55f * sinf(spin + 3.2f) * 0.5f);
            glEnd();
        }
    }
}

void DrawSnowFar4()  { DrawSnowBand4(0,              SNOW_FAR_END4, 0.55f,  95, false); }
void DrawSnowMid4()  { DrawSnowBand4(SNOW_FAR_END4,  SNOW_MID_END4, 1.00f, 205, false); }
void DrawSnowNear4() { DrawSnowBand4(SNOW_MID_END4,  MAX_SNOW4,     1.95f, 215, true ); }

void UpdateSnow4(int) {
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateSnow4, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating4) {
        // Settled snow eases toward the depth implied by the intensity key.
        float tgt = SnowCoverTarget4();
        snowCover4 += (tgt - snowCover4) * 0.02f;

        float base = 0.22f + snowIntensity4 * 0.14f;
        for (int i = 0; i < MAX_SNOW4; i++) {
            // Near flakes fall visibly faster than far ones -- motion
            // parallax, which is what actually sells the depth.
            float bandSpeed = (i < SNOW_FAR_END4) ? 0.50f
                            : (i < SNOW_MID_END4) ? 1.00f : 1.65f;
            snowY4[i] -= base * bandSpeed;
            snowX4[i] += snowDrift4[i] * bandSpeed;
            if (snowY4[i] < -40.0f) {
                snowY4[i] = 40.0f;
                snowX4[i] = -62.0f + (rand()%1240)/10.0f;
            }
            if (snowX4[i] >  63.0f) snowX4[i] = -63.0f;
            if (snowX4[i] < -63.0f) snowX4[i] =  63.0f;
        }
    }
    glutTimerFunc(25, UpdateSnow4, 0);
}

// ============================================================================
//  CONTRACT: Init / Draw / Keyboard / Mouse / kTitle
// ============================================================================
const char* kTitle = "Riverfront Market Life";

void Init() {
    // ---- Star field -------------------------------------------------------
    // Mostly faint, small and slow; the size bucket and the brightness are
    // rolled together so a big star is also a bright one. Colours drift
    // between a cold blue-white and a warm cream, which is what keeps a field
    // this dense from looking like a spray of identical pixels.
    for (int i = 0; i < NUM_STARS4; i++) {
        stars4[i].x = -60.0f + (rand()%1200)/10.0f;
        stars4[i].y =   4.0f + (rand()%350)/10.0f;
        stars4[i].twinkle = (rand()%628)/100.0f;
        stars4[i].rate    = 0.9f + (rand()%220)/100.0f;
        int roll = rand() % 100;
        if      (roll < 62) { stars4[i].bucket = 0; stars4[i].mag = 0.22f + (rand()%26)/100.0f; }
        else if (roll < 90) { stars4[i].bucket = 1; stars4[i].mag = 0.45f + (rand()%32)/100.0f; }
        else                { stars4[i].bucket = 2; stars4[i].mag = 0.72f + (rand()%28)/100.0f; }
        int tint = rand() % 100;
        if      (tint < 55) { stars4[i].r = 255; stars4[i].g = 255; stars4[i].b = 255; }
        else if (tint < 80) { stars4[i].r = 206; stars4[i].g = 222; stars4[i].b = 255; }
        else if (tint < 93) { stars4[i].r = 255; stars4[i].g = 244; stars4[i].b = 214; }
        else                { stars4[i].r = 255; stars4[i].g = 216; stars4[i].b = 196; }
    }
    // The bright few, kept clear of the moon so they do not sit inside its
    // halo where nothing would be visible anyway.
    for (int i = 0; i < NUM_BRIGHT_STARS4; i++) {
        float bx, by;
        int guard = 0;
        do {
            bx = -56.0f + (rand()%1120)/10.0f;
            by =  12.0f + (rand()%240)/10.0f;
        } while (++guard < 20 &&
                 (bx-moonX4)*(bx-moonX4) + (by-moonY4)*(by-moonY4) < 100.0f);
        brightStars4[i].x = bx;
        brightStars4[i].y = by;
        brightStars4[i].r = 0.28f + (rand()%26)/100.0f;
        brightStars4[i].phase = (rand()%628)/100.0f;
    }
    // Birds start scattered across the sky rather than all entering together.
    for (int i = 0; i < NUM_BIRDS4; i++) {
        birds4[i].dir = (i % 4 == 0) ? -1 : 1;
        RespawnBird4(birds4[i], -66.0f + (rand()%1320)/10.0f);
    }
    for (int i = 0; i < NUM_SHOPS4; i++)
        for (int w = 0; w < 5; w++) shops4[i].lit[w] = (rand()%100) < 60;
    for (int s = 0; s < NUM_LIGHT_SPANS4; s++)
        for (int b = 0; b < BULBS_PER_SPAN4; b++) bulbLit4[s][b] = (rand()%100) < 70;
    for (int i = 0; i < MAX_SMOKE4; i++) chimneySmoke4[i].active = false;
    for (int i = 0; i < MAX_STEAM4; i++) stallSteam4[i].active = false;
    for (int i = 0; i < MAX_FIREWORKS4; i++) fireworks4[i].state = FW_INACTIVE;
    for (int i = 0; i < MAX_EMBERS4; i++)    embers4[i].active = false;
    snowCover4 = SnowCoverTarget4();
    InitSnow4();

    glutTimerFunc(0, UpdateSnow4, 0);
    glutTimerFunc(0, UpdateFerrisWheel4, 0);
    glutTimerFunc(0, UpdateTwinkle4, 0);
    glutTimerFunc(0, UpdateSkaters4, 0);
    glutTimerFunc(0, UpdateSled4, 0);
    glutTimerFunc(0, UpdatePedestrians4, 0);
    glutTimerFunc(0, UpdateFire4, 0);
    glutTimerFunc(0, UpdateStallSteam4, 0);
    glutTimerFunc(0, UpdateChimneySmoke4, 0);
    glutTimerFunc(0, UpdateSleigh4, 0);
    glutTimerFunc(0, UpdateBirds4, 0);
    glutTimerFunc(0, UpdateMetroRail4, 0);
    glutTimerFunc(0, UpdatePendulum4, 0);
    glutTimerFunc(0, UpdateFireworks4, 0);
    glutTimerFunc(0, UpdateAurora4, 0);
    glutTimerFunc(0, UpdateSledRun4, 0);
    glutTimerFunc(0, UpdateFog4, 0);
    glutTimerFunc(0, UpdateRickshaw4, 0);
    glutTimerFunc(0, UpdateCar4, 0);
    glutTimerFunc(0, UpdateBike4, 0);
    glutTimerFunc(0, UpdateSeason4, 0);
    glutTimerFunc(0, UpdateBoats4, 0);              // RIVERSIDE (added)
    glutTimerFunc(0, UpdateRiverPedestrians4, 0);   // RIVERSIDE (added)
}

void Draw() {
    glClearColor(0.02f, 0.02f, 0.06f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(WORLD_LEFT, WORLD_RIGHT, WORLD_BOTTOM, WORLD_TOP, -10, 10);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    DrawSky4();
    DrawStars4();
    DrawMoon4();
    DrawAurora4();
    DrawBirds4();                // distant birds: sky only, cropped by the roofs
    DrawSnowFar4();             // far band: behind the whole market
    DrawDistantBuildings4();     // dim skyline behind the market
    DrawRoadsideBlocks4();       // the street canyon framing the new road
    DrawMetroRail4();            // elevated metro rail behind the shops
    DrawFireworks4();
    DrawSleigh4();
    DrawShops4();
    DrawChimney4();
    DrawClockTower4();
    // Plaza lamps in front of the shop row (varying scale for perspective)
    DrawPlazaLamp4(-10.0f, 0.90f);
    DrawPlazaLamp4(  6.0f, 0.72f);
    DrawPlazaLamp4( 30.0f, 0.58f);
    DrawLightSpans4();

    DrawFireworkGroundFlash4();
    DrawPlazaGround4();
    DrawPerspectiveRoad4();     // the side street, part of the ground itself

    // The wheel's feet are on y = -10, so everything standing NEARER than
    // that -- the stalls, the crowd, the rink -- has to draw after it. It
    // used to be drawn last of the plaza, which put a structure standing at
    // y = -10 in front of walkers standing at y = -13.9.
    DrawFerrisWheel4();

    DrawBuildingShadows4();     // what the back row lays on the snow
    DrawLightPools4();          // every light source spills onto the snow
    DrawSnowAccumulation4();
    DrawRink4();
    DrawFountainAutumn4();      // takes the rink's ground out of season
    DrawIceSculptures4();
    DrawStage4();
    DrawStalls4();
    DrawFirePit4();
    DrawChristmasTree4();
    DrawGiftBoxes4();
    DrawSanta4();
    DrawChestnutRoaster4();     // stands where Santa does, out of season
    DrawSnowmanFamily4();
    DrawPlantersAutumn4();      // takes the snowmen's ground out of season
    DrawNutcracker4(-9.0f);
    DrawSkaters4();
    DrawSled4();
    DrawChimneySmoke4();
    DrawStallSteam4();
    DrawCarolers4();
    DrawPedestrians4();
    // Small dog playing on the main plaza at the right side, away from the
    // river and the terrace/footpath. Increased to 2x the previous size.
    DrawDogPlay4(32.0f, -15.8f, 1.20f, pedTimer4);
    DrawTeaStall4();            // roadside tea stall in the empty near band

    // The rickshaw runs from the horizon down to the terrace coping, so for
    // most of its journey it is nearer the camera than the plaza crowd. Drawn
    // here it correctly passes in front of them down the near half of the
    // street, which is where it spends most of its run.
    // Three vehicles on one street, so they must be drawn in depth order:
    // whichever is furthest down the road is nearest the camera and goes last.
    {
        struct RoadUser { float t; void (*draw)(); };
        RoadUser users[3] = { { rickshawT4, DrawRickshaw4 },
                              { carT4,      DrawCar4      },
                              { bikeT4,     DrawBike4     } };
        for (int i = 1; i < 3; i++) {          // tiny insertion sort by t
            RoadUser key = users[i];
            int j = i - 1;
            while (j >= 0 && users[j].t > key.t) { users[j+1] = users[j]; j--; }
            users[j+1] = key;
        }
        for (int i = 0; i < 3; i++) users[i].draw();
    }

    DrawFog4(-6.0f, 14.0f, 105);   // haze hangs over the market itself
    DrawSnowMid4();             // mid band: between the market and the terrace

    // ---- NEAR TERRACE (foreground) -------------------------------------
    DrawTerraceWall4();
    DrawSledRun4();
    // ---- RIVERSIDE (added) ---------------------------------------------
    // Depth order below: stairs -> river/reflections -> boats -> the
    // unloading vignette -> people walking the dock. This replaces the old
    // single DrawFrozenCanal4() call; see the RIVERSIDE comment block above
    // DrawStairs4() for how to undo just this change.
    DrawStairsAll4();
    DrawRiver4();
    DrawBoats4();
    DrawGoodsUnloading4();
    DrawRiverPedestrians4();
    // ----------------------------------------------------------------------
    DrawForegroundCrowd4();
    DrawForegroundLamp4(-58.0f, 0.90f);
    DrawForegroundLamp4( 56.0f, 0.78f);
    DrawFog4(-40.0f, -20.0f, 70);  // thinner haze across the near terrace

    DrawSnowNear4();            // near band: big, bright, in front of everything

    static const char* const hud[] = {
        "1 light snow   2 medium   3 heavy",
        "4 daytime      5 night (season is kept)",
        "S  winter / autumn",
        "L  warm / multicolour lights    F  snow squall",
        "X  set off the sled, fireworks and aurora",
        "SPACE pause    H help    N/B change scene    ESC quit",
        nullptr
    };
    DrawSceneHUD(kTitle, hud);
}

void Keyboard(unsigned char key, int /*x*/, int /*y*/) {
    switch (key) {
        case '1': snowIntensity4 = 0; break;
        case '2': snowIntensity4 = 1; break;
        case '3': snowIntensity4 = 2; break;
        case 'l': case 'L': multicolorLights4 = !multicolorLights4; break;
        case 'f': case 'F': fogMode4 = !fogMode4; break;

        // Season. 1-5, L, F and X were all taken, so S it is.
        case 's': case 'S':
            seasonTarget4 = (seasonTarget4 > 0.5f) ? 0.0f : 1.0f;
            break;

        // Time of day. 4 brings the sun up, 5 puts the scene back to night.
        // It clears the two mood toggles, but it deliberately does NOT touch
        // seasonTarget4: the season is the viewer's own choice and switching
        // the lights off at dusk should not drag autumn back into winter.
        case '4': dayTarget4 = 1.0f; break;
        case '5':
            dayTarget4        = 0.0f;
            multicolorLights4 = false;
            fogMode4          = false;
            break;

        // The rare events are genuinely rare -- the sled has a 300-tick
        // cooldown and the aurora a 600-tick one -- so during a live demo
        // they may simply never fire while anyone is watching. 'X' forces
        // all three at once.
        case 'x': case 'X':
            sledActive4 = true;  sledX4 = -75.0f;
            auroraActive4 = true; auroraTimer4 = 0.0f;
            fireworkCooldown4 = 1;
            break;
    }
    glutPostRedisplay();
}

void Mouse(int button, int state, int /*x*/, int /*y*/) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)  isAnimating4 = true;
    if (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN) isAnimating4 = false;
    glutPostRedisplay();
}

} // namespace Scenario4
// ============================================================================
//  SCENARIO NAVIGATION
// ----------------------------------------------------------------------------
//  The program used to open on a clickable home menu. That menu is gone: the
//  window now opens directly on Scenario 1 and the viewer walks the scenes
//  with the keyboard.
//
//      N / RIGHT ARROW   next scenario      (4 wraps back round to 1)
//      B / LEFT ARROW    previous scenario  (1 wraps back round to 4)
//      F1 .. F4          jump straight to that scenario
//
//  Why two mechanisms, and why these keys: 1/2/3/4 are already spoken for
//  inside every scenario (day phase, weather, snow depth), so the numbers
//  cannot be reused for navigation without breaking those controls. N and B
//  give the natural presentation order; F1-F4 arrive through glutSpecialFunc,
//  a separate GLUT callback this program does not otherwise use, so direct
//  jumps are possible with no risk of colliding with any scenario's keys.
//
//  STANDALONE BUILD: the whole mechanism below is kept exactly as it is so
//  that this file and CityLife.cpp stay diff-clean. With one scene in the
//  table every one of those keys resolves to index 0, i.e. this scene, and
//  simply replays its title card.
// ============================================================================
struct ScenarioInfo { const char* title; const char* subtitle; AppScreen screen; };

// Titles come straight from each scenario's own kTitle, so renaming a scene
// in its namespace updates the title card automatically.
// STANDALONE BUILD: one row instead of four. GoToScenarioIndex() wraps any
// index back onto row 0, so N / B / F1-F4 stay harmless rather than needing
// to be unbound.
ScenarioInfo scenarios[NUM_SCENARIOS] = {
    { Scenario4::kTitle,             "Snowy night market - ferris wheel, sleigh, fog", SCENARIO_4 },
};

// ---- Transition state ------------------------------------------------------
//  Switching used to be a hard cut between two completely different scenes.
//  `switchFade` dips the screen through black across the change, and
//  `titleCardT` runs a name card in afterwards -- between them they do the
//  job the menu used to do, which was telling the viewer what they are
//  looking at.
float switchFade  = 0.0f;    // 1 = fully black, decays to 0
float titleCardT  = 0.0f;    // counts up from 0; card is visible below ~2.6s
bool  firstLaunch = true;    // the opening card lingers and adds a key hint

// `initial` is true only for the opening call from main(). That card holds
// longer and spells out the navigation keys; once the viewer has moved for
// themselves the hint has done its job and every later card is short.
void GoToScenarioIndex(int index, bool initial = false) {
    if (index < 0) index = NUM_SCENARIOS - 1;
    if (index >= NUM_SCENARIOS) index = 0;

    currentScenarioIndex = index;
    currentScreen = scenarios[index].screen;

    isPaused    = false;       // never carry a pause into the next scene
    switchFade  = initial ? 0.0f : 1.0f;   // no fade-in from black on launch
    titleCardT  = 0.0f;
    firstLaunch = initial;

    // The window title bar follows the scene, which helps when the demo is
    // being screen-recorded.
    static char windowTitle[160];
    sprintf(windowTitle, "City Life  -  %d/%d  %s",
            index + 1, NUM_SCENARIOS, scenarios[index].title);
    glutSetWindowTitle(windowTitle);

    glutPostRedisplay();
}

void NextScenario()     { GoToScenarioIndex(currentScenarioIndex + 1); }
void PreviousScenario() { GoToScenarioIndex(currentScenarioIndex - 1); }

// ---- Title card ------------------------------------------------------------
//  Scenario name and one-line description, fading in and then out again.
void DrawTitleCard() {
    float hold = firstLaunch ? 4.2f : 2.6f;
    if (titleCardT > hold) return;

    // Fade in over the first 0.35s, hold, then fade out over the last 0.7s.
    float a = 1.0f;
    if (titleCardT < 0.35f)        a = titleCardT / 0.35f;
    else if (titleCardT > hold - 0.7f) a = (hold - titleCardT) / 0.7f;
    if (a < 0.0f) a = 0.0f;
    if (a > 1.0f) a = 1.0f;

    const ScenarioInfo& sc = scenarios[currentScenarioIndex];

    // Dark plate behind the text so the card reads over any scene, bright
    // daytime park or black winter sky alike.
    glColor4f(0.04f, 0.05f, 0.08f, 0.62f * a);
    glBegin(GL_QUADS);
        glVertex2f(WORLD_LEFT,  6.0f);  glVertex2f(WORLD_RIGHT, 6.0f);
        glVertex2f(WORLD_RIGHT, 20.0f); glVertex2f(WORLD_LEFT,  20.0f);
    glEnd();

    // Hairlines top and bottom
    glColor4f(0.85f, 0.88f, 0.96f, 0.55f * a);
    glLineWidth(1.4f);
    glBegin(GL_LINES);
        glVertex2f(-34.0f, 19.4f); glVertex2f(34.0f, 19.4f);
        glVertex2f(-34.0f,  6.6f); glVertex2f(34.0f,  6.6f);
    glEnd();
    glLineWidth(1.0f);

    char counter[48];
    sprintf(counter, "SCENARIO %d OF %d", currentScenarioIndex + 1, NUM_SCENARIOS);

    glColor4f(0.62f, 0.72f, 0.88f, a);
    DrawTextCentered(0.0f, 16.4f, GLUT_BITMAP_HELVETICA_12, counter);

    glColor4f(1.0f, 1.0f, 1.0f, a);
    DrawTextCentered(0.0f, 12.4f, GLUT_BITMAP_TIMES_ROMAN_24, sc.title);

    glColor4f(0.78f, 0.82f, 0.90f, a);
    DrawTextCentered(0.0f, 8.8f, GLUT_BITMAP_HELVETICA_12, sc.subtitle);

    // On the very first card only, spell out how to move on. After that the
    // permanent HUD line carries it.
    if (firstLaunch) {
        glColor4f(0.95f, 0.86f, 0.55f, a);
        DrawTextCentered(0.0f, 3.2f, GLUT_BITMAP_HELVETICA_12,
                         "press  N  for the next scenario     F1-F4 to jump     H for controls");
    }
}

// ---- Cross-fade ------------------------------------------------------------
void DrawSwitchFade() {
    if (switchFade <= 0.01f) return;
    glColor4f(0.0f, 0.0f, 0.0f, switchFade);
    glBegin(GL_QUADS);
        glVertex2f(WORLD_LEFT,  WORLD_BOTTOM); glVertex2f(WORLD_RIGHT, WORLD_BOTTOM);
        glVertex2f(WORLD_RIGHT, WORLD_TOP);    glVertex2f(WORLD_LEFT,  WORLD_TOP);
    glEnd();
}

// Advances both effects. Driven from the master tick, so it runs at a fixed
// rate regardless of what the active scenario is doing.
void UpdateTransition(float dt) {
    if (switchFade > 0.0f) {
        switchFade -= dt * 2.9f;                 // ~0.35s to clear
        if (switchFade < 0.0f) switchFade = 0.0f;
    }
    titleCardT += dt;
}

// ============================================================================
// ============================================================================
//  MASTER CLOCK
// ----------------------------------------------------------------------------
//  Every animation used to end with its own glutPostRedisplay(). With 13-24
//  timers live in the active scenario, all firing on ~30 ms, GLUT was being
//  asked to redraw roughly 600 times a second and the main loop spun as fast
//  as the machine allowed -- lots of heat, no extra smoothness, and a frame
//  rate that varied with whichever scenario was on screen.
//
//  Now the timers only advance state. This single tick issues exactly one
//  redisplay per frame, which pins the whole program to a steady ~62 fps on
//  every machine without touching any animation's speed.
// ============================================================================
const unsigned int FRAME_MS = 16;   // ~62.5 frames per second

void MasterTick(int) {
    UpdateTransition(FRAME_MS / 1000.0f);
    glutPostRedisplay();
    glutTimerFunc(FRAME_MS, MasterTick, 0);
}

void display() {
    switch (currentScreen) {
        case SCENARIO_4: Scenario4::Draw();               break;
        default: break;   // standalone build: the other three aren't compiled in
    }

    // Drawn over whichever scene just rendered, in its own clean projection
    // so a scenario that changed the ortho box cannot shift the overlay.
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(WORLD_LEFT, WORLD_RIGHT, WORLD_BOTTOM, WORLD_TOP, -10, 10);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    DrawTitleCard();
    DrawSwitchFade();

    glutSwapBuffers();
}

void keyboard(unsigned char key, int x, int y) {
    // ---- Navigation -------------------------------------------------------
    // N and B walk the scenes in order. They are letters, not numbers,
    // because 1/2/3/4 belong to whichever scenario is on screen.
    if (key == 'n' || key == 'N') { NextScenario();     return; }
    if (key == 'b' || key == 'B') { PreviousScenario(); return; }

    // ESC used to return to the home menu. With no menu to return to it
    // would be a dead key, so it now closes the program -- which is what
    // anyone sitting down in front of this will press and expect.
    if (key == 27) {
        exit(0);
    }

    // ---- Controls that mean the same thing in every scenario --------------
    if (key == ' ') {       // SPACE: pause / resume
        isPaused = !isPaused;
        glutPostRedisplay();
        return;
    }
    if (key == 'h' || key == 'H') {  // H: show / hide the help panel
        showHelp = !showHelp;
        glutPostRedisplay();
        return;
    }

    switch (currentScreen) {
        case SCENARIO_4: Scenario4::Keyboard(key, x, y);              break;
        default: break;   // standalone build: the other three aren't compiled in
    }
}

// ---- Special keys ----------------------------------------------------------
//  F1-F4 jump straight to a scenario and the arrow keys mirror N and B.
//  These arrive through a different GLUT callback from the ordinary keyboard
//  handler, so they cannot clash with any scenario's own controls.
void special(int key, int /*x*/, int /*y*/) {
    switch (key) {
        case GLUT_KEY_F1: GoToScenarioIndex(0); break;
        case GLUT_KEY_F2: GoToScenarioIndex(1); break;
        case GLUT_KEY_F3: GoToScenarioIndex(2); break;
        case GLUT_KEY_F4: GoToScenarioIndex(3); break;
        case GLUT_KEY_RIGHT: NextScenario();     break;
        case GLUT_KEY_LEFT:  PreviousScenario(); break;
        default: break;
    }
}

void mouse(int button, int state, int x, int y) {
    switch (currentScreen) {
        case SCENARIO_4: Scenario4::Mouse(button, state, x, y);              break;
        default: break;   // standalone build: the other three aren't compiled in
    }
}

// ============================================================================
//  SHARED ONE-TIME GL SETUP + ENTRY POINT
// ============================================================================
void init() {
    glPointSize(2.0f);
    glLineWidth(2.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // Smooths every circle, sail, kite and rooftop edge in the project. The
    // buffer is requested in main() via GLUT_MULTISAMPLE; enabling it here is
    // harmless on drivers that could not supply one.
    glEnable(GL_MULTISAMPLE);
}

// ---- Window resize ---------------------------------------------------------
// Without this, resizing stretches every scene and breaks DrawTextCentered(),
// which converts pixel widths using the fixed WINDOW_WIDTH. The viewport is
// letterboxed so the world box keeps its 3:2 shape at any window size.
void reshape(int w, int h) {
    if (w <= 0 || h <= 0) return;

    const float targetAspect = (WORLD_RIGHT - WORLD_LEFT) / (WORLD_TOP - WORLD_BOTTOM);
    float windowAspect = (float)w / (float)h;

    int vpW = w, vpH = h, vpX = 0, vpY = 0;
    if (windowAspect > targetAspect) {         // too wide: bars left and right
        vpW = (int)(h * targetAspect);
        vpX = (w - vpW) / 2;
    } else {                                   // too tall: bars top and bottom
        vpH = (int)(w / targetAspect);
        vpY = (h - vpH) / 2;
    }
    glViewport(vpX, vpY, vpW, vpH);
    viewportPixelWidth = vpW;                  // text centring follows the viewport
    glutPostRedisplay();
}

int main(int argc, char** argv) {
    // Without a seed every run produces the identical star field, window
    // pattern and firework timing.
    srand((unsigned int)time(nullptr));

    glutInit(&argc, argv);
    // GLUT_MULTISAMPLE asks for an anti-aliased buffer; drivers that cannot
    // supply one simply ignore it, so this is safe everywhere.
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_MULTISAMPLE);
    glutInitWindowPosition(100, 100);
    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    glutCreateWindow("City Life - Scenario 4 - Riverfront Market Life");

    init();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    // F1-F4 and the arrow keys. Remove this one line if the navigation
    // should be N / B only.
    glutSpecialFunc(special);
    glutMouseFunc(mouse);

    // One redisplay per frame for the whole program (see MasterTick).
    glutTimerFunc(0, MasterTick, 0);

    // Each scenario's own one-time setup (also starts that scenario's
    // animation timers, so all four keep animating in the background
    // regardless of which one is currently on screen).
    // STANDALONE BUILD: only one left to set up.
    Scenario4::Init();

    // Open on Scenario 4, with its title card up and the key hint showing.
    GoToScenarioIndex(0, true);

    glutMainLoop();
    return 0;
}