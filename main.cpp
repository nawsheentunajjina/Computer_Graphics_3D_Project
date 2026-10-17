//
//  main.cpp
//  Premium Living Room - Computer Graphics Lab Project
//
//  Requirements covered:
//    1. 3D transformation      -> every piece of furniture/decor is built
//                                 from translate/scale (and rotate for the
//                                 fan and the sliding door track)
//    2. Viewing transformation -> free-fly Camera class (WASD + mouse look)
//    3. Moving objects         -> rotating ceiling fan (F) + sliding glass
//                                 door (G)
//    4. Two kinds of light     -> 2 point lights + 1 spotlight (toggle 1/2)
//    5. Different colors       -> navy sofa, mustard cushions, terracotta
//                                 rug, emerald accent wall, brass fixtures,
//                                 walnut wood tones, colorful books, etc.
//
//  Controls:
//    W A S D        - move around
//    Mouse          - look around
//    Arrow keys     - also turn/look around (backup if mouse-look
//                     doesn't behave on your machine)
//    SPACE / LCTRL  - move up / down
//    Mouse wheel    - zoom in / out (or hold Z / X)
//    = / -          - raise / lower the brightness of all room lights
//    1              - toggle the two point lights (ceiling + side lamp)
//    2              - toggle the spotlight (floor lamp)
//    F              - fan on/off (spins up and down smoothly)
//    G              - open / close the sliding glass door
//    ESC            - quit
//

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "shader.h"
#include "camera.h"
#include "pointLight.h"
#include "spotLight.h"

#include <iostream>

using namespace std;

// ---------------------------------------------------------
// forward declarations
// ---------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);

// ---------------------------------------------------------
// settings
// ---------------------------------------------------------
const unsigned int SCR_WIDTH = 1200;
const unsigned int SCR_HEIGHT = 800;

// ---------------------------------------------------------
// camera
// ---------------------------------------------------------
Camera camera(glm::vec3(0.0f, 1.5f, -6.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// ---------------------------------------------------------
// lights
// ---------------------------------------------------------
// point light 1: warm ceiling pendant above the coffee table
PointLight pointlight1(
    0.0f, 2.75f, 1.0f,          // position
    0.06f, 0.16f, 0.14f,        // ambient
    1.0f, 0.92f, 0.75f,         // diffuse (warm white)
    1.0f, 1.0f, 1.0f,           // specular
    1.0f, 0.07f, 0.022f,        // k_c, k_l, k_q (bit longer reach)
    1                            // light number
);

// point light 2: amber side-table lamp accent light
PointLight pointlight2(
    1.3f, 0.95f, 2.5f,          // position (beside the sectional's right armrest)
    0.12f, 0.10f, 0.07f,        // ambient
    1.0f, 0.76f, 0.42f,         // diffuse (amber)
    0.6f, 0.5f, 0.3f,           // specular
    1.0f, 0.09f, 0.032f,        // k_c, k_l, k_q (normal range, was too short before)
    2                            // light number
);

// spotlight: cool-white floor lamp, standing just in front of the
// chaise's outer tip (classic spot for a reading lamp beside a
// chaise lounge). Positioned just BELOW the lamp shade (in open
// air) instead of inside it - a light source sealed inside solid
// geometry can't visibly show its own glow.
SpotLight spotlight(
    glm::vec3(-2.55f, 1.58f, 0.7f),         // position
    glm::vec3(0.3f, -1.0f, 0.7f),           // direction (down & back toward the chaise/rug)
    glm::vec3(0.07f, 0.07f, 0.08f),        // ambient
    glm::vec3(0.80f, 0.85f, 1.0f),         // diffuse (cool bluish white)
    glm::vec3(0.6f, 0.6f, 0.7f),           // specular
    1.0f, 0.06f, 0.017f,                    // k_c, k_l, k_q (longer reach)
    18.0f, 28.0f                            // inner / outer cone angles (wider)
);

bool pointLightsOn = true;
bool spotLightOn = true;
float lightIntensity = 1.0f; // overall brightness multiplier for all room lights (adjust with = / -)

// ---------------------------------------------------------
// animated objects
// ---------------------------------------------------------
float fanAngle = 0.0f;
float fanSpeed = 0.0f;              // current angular speed (deg/sec) - ramps up/down
const float fanMaxSpeed = 800.0f;   // top speed once fully spun up
const float fanAccel = 90.0f;       // how fast it speeds up / slows down (deg/sec^2)
bool fanSpinning = true;

float doorSlide = 0.0f;   // 0 = closed, grows as the door slides open
bool doorOpen = false;

// ---------------------------------------------------------
// timing
// ---------------------------------------------------------
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// ===========================================================
// low-level cube drawing (uses the lighting shader's Material)
// ===========================================================
void drawCube(unsigned int& cubeVAO, Shader& lightingShader, glm::mat4 model, float r, float g, float b)
{
    lightingShader.use();

    lightingShader.setVec3("material.ambient", glm::vec3(r, g, b));
    lightingShader.setVec3("material.diffuse", glm::vec3(r, g, b));
    lightingShader.setVec3("material.specular", glm::vec3(0.5f, 0.5f, 0.5f));
    lightingShader.setFloat("material.shininess", 32.0f);

    lightingShader.setMat4("model", model);

    glBindVertexArray(cubeVAO);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
}

// ===========================================================
// placeCube: draws a box whose ORIGIN is its bottom-center point.
// pos     - world (or local, if a root is given) position of the
//           bottom-center of the box
// size    - width (x), height (y), depth (z)
// color   - rgb in [0,1]
// root    - optional group transform (for furniture built in local
//           space and then moved/rotated into the room as a unit)
// ===========================================================
void placeCube(unsigned int& cubeVAO, Shader& shader, glm::vec3 pos, glm::vec3 size, glm::vec3 color, glm::mat4 root = glm::mat4(1.0f))
{
    glm::mat4 T = glm::translate(glm::mat4(1.0f), pos);
    glm::mat4 S = glm::scale(glm::mat4(1.0f), size);
    glm::mat4 C = glm::translate(glm::mat4(1.0f), glm::vec3(-0.5f, 0.0f, -0.5f));
    glm::mat4 model = root * T * S * C;
    drawCube(cubeVAO, shader, model, color.x, color.y, color.z);
}

// ===========================================================
// Room shell: floor, ceiling, 3 walls (front left open for the
// camera) and a small wall-mounted painting. The back wall's
// window opening is filled by the sliding glass door instead.
// ===========================================================
void drawRoom(unsigned int& cubeVAO, Shader& shader)
{
    glm::vec3 wallColor(0.85f, 0.80f, 0.72f);      // warm greige
    glm::vec3 accentWallColor(0.58f, 0.70f, 0.62f); // soft, light sage accent wall

    // floor (top surface sits exactly at y = 0)
    placeCube(cubeVAO, shader, glm::vec3(0.0f, -0.06f, 0.0f), glm::vec3(7.0f, 0.06f, 7.0f), glm::vec3(0.36f, 0.24f, 0.15f));

    // ceiling (underside sits at y = 3.0)
    placeCube(cubeVAO, shader, glm::vec3(0.0f, 3.0f, 0.0f), glm::vec3(7.0f, 0.06f, 7.0f), glm::vec3(0.95f, 0.95f, 0.92f));

    // back wall (+Z)
    placeCube(cubeVAO, shader, glm::vec3(0.0f, 0.0f, 3.44f), glm::vec3(7.0f, 3.0f, 0.12f), wallColor);

    // left wall (-X)
    placeCube(cubeVAO, shader, glm::vec3(-3.44f, 0.0f, 0.0f), glm::vec3(0.12f, 3.0f, 7.0f), wallColor);

    // right accent wall (+X)
    placeCube(cubeVAO, shader, glm::vec3(3.44f, 0.0f, 0.0f), glm::vec3(0.12f, 3.0f, 7.0f), accentWallColor);

    // wall art on back wall (frame + canvas), above the side table
    placeCube(cubeVAO, shader, glm::vec3(-2.3f, 1.8f, 3.32f), glm::vec3(0.95f, 0.75f, 0.06f), glm::vec3(0.20f, 0.12f, 0.08f));
    placeCube(cubeVAO, shader, glm::vec3(-2.3f, 1.8f, 3.36f), glm::vec3(0.8f, 0.6f, 0.03f), glm::vec3(0.82f, 0.36f, 0.26f));
}

// ===========================================================
// Crown molding + baseboard trim on the left and right walls
// (skipped on the back wall, which carries the sliding door)
// ===========================================================
void drawTrim(unsigned int& cubeVAO, Shader& shader)
{
    glm::vec3 crown(0.97f, 0.96f, 0.92f);
    glm::vec3 base(0.15f, 0.09f, 0.05f);

    // left wall
    placeCube(cubeVAO, shader, glm::vec3(-3.34f, 2.82f, 0.0f), glm::vec3(0.06f, 0.14f, 7.0f), crown);
    placeCube(cubeVAO, shader, glm::vec3(-3.34f, -0.02f, 0.0f), glm::vec3(0.06f, 0.14f, 7.0f), base);

    // right accent wall
    placeCube(cubeVAO, shader, glm::vec3(3.34f, 2.82f, 0.0f), glm::vec3(0.06f, 0.14f, 7.0f), crown);
    placeCube(cubeVAO, shader, glm::vec3(3.34f, -0.02f, 0.0f), glm::vec3(0.06f, 0.14f, 7.0f), base);
}

// ===========================================================
// Sliding glass door on the back wall (replaces the old window).
// One panel is fixed; the other slides sideways by `slideOffset`
// when opened. Curtains + a rod frame it for a premium look.
// ===========================================================
void drawSlidingDoor(unsigned int& cubeVAO, Shader& shader, float slideOffset)
{
    float centerX = 1.6f;
    float doorHeight = 2.2f;
    float panelWidth = 2.3f;

    glm::vec3 frameColor(0.08f, 0.08f, 0.09f); // black/bronze frame
    glm::vec3 glassColor(0.72f, 0.85f, 0.92f);
    glm::vec3 curtainColor(0.90f, 0.87f, 0.80f);
    glm::vec3 handleColor(0.72f, 0.60f, 0.20f);

    // track (top and bottom)
    placeCube(cubeVAO, shader, glm::vec3(centerX, doorHeight, 3.34f), glm::vec3(2.7f, 0.06f, 0.10f), frameColor);
    placeCube(cubeVAO, shader, glm::vec3(centerX, 0.0f, 3.34f), glm::vec3(2.7f, 0.04f, 0.10f), frameColor);

    // ---- fixed panel (left half) ----
    float fixedX = centerX - panelWidth / 2.0f;
    placeCube(cubeVAO, shader, glm::vec3(fixedX, 0.0f, 3.30f), glm::vec3(panelWidth, doorHeight, 0.04f), glassColor);
    // grid mullions for a "French door" look
    placeCube(cubeVAO, shader, glm::vec3(fixedX, doorHeight * 0.5f, 3.32f), glm::vec3(panelWidth, 0.05f, 0.02f), frameColor);
    placeCube(cubeVAO, shader, glm::vec3(fixedX, 0.0f, 3.32f), glm::vec3(0.05f, doorHeight, 0.02f), frameColor);

    // ---- sliding panel (right half) - animates with slideOffset ----
    float slidingX = centerX + panelWidth / 2.0f + slideOffset;
    placeCube(cubeVAO, shader, glm::vec3(slidingX, 0.0f, 3.30f), glm::vec3(panelWidth, doorHeight, 0.04f), glassColor);
    placeCube(cubeVAO, shader, glm::vec3(slidingX, doorHeight * 0.5f, 3.32f), glm::vec3(panelWidth, 0.05f, 0.02f), frameColor);
    placeCube(cubeVAO, shader, glm::vec3(slidingX, 0.0f, 3.32f), glm::vec3(0.05f, doorHeight, 0.02f), frameColor);
    // handle on the sliding panel's leading edge
    placeCube(cubeVAO, shader, glm::vec3(slidingX - panelWidth / 2.0f + 0.04f, 0.8f, 3.345f), glm::vec3(0.03f, 0.5f, 0.03f), handleColor);

    // curtain rod
    placeCube(cubeVAO, shader, glm::vec3(centerX, doorHeight + 0.15f, 3.20f), glm::vec3(3.0f, 0.05f, 0.05f), frameColor);
    // curtain panels on both sides
    placeCube(cubeVAO, shader, glm::vec3(centerX - 1.45f, 0.0f, 3.18f), glm::vec3(0.35f, doorHeight + 0.1f, 0.06f), curtainColor);
    placeCube(cubeVAO, shader, glm::vec3(centerX + 1.45f, 0.0f, 3.18f), glm::vec3(0.35f, doorHeight + 0.1f, 0.06f), curtainColor);
}

// ===========================================================
// L-shaped SECTIONAL sofa (like the reference photo): a long
// main run with a right-side armrest, plus a lower chaise
// extension on the left that juts further into the room.
// Local origin sits at the inner corner where the two meet:
//   - main run:  local x in [0, 2.8],  z in [0, 0.9]  (back at z=0.9)
//   - chaise:    local x in [-1.1, 0], z in [-0.9, 0.9]
// Both sit against the same wall (z=0.9 side), so no separate
// chaise backrest is needed - the wall itself reads as the back.
// ===========================================================
void drawSectionalSofa(unsigned int& cubeVAO, Shader& shader, glm::mat4 root,
    glm::vec3 baseColor = glm::vec3(0.15f, 0.24f, 0.44f),
    glm::vec3 cushionColor = glm::vec3(0.80f, 0.60f, 0.20f),
    glm::vec3 pillowColorA = glm::vec3(0.75f, 0.35f, 0.25f),
    glm::vec3 pillowColorB = glm::vec3(0.85f, 0.65f, 0.20f))
{
    // ---- main run ----
    placeCube(cubeVAO, shader, glm::vec3(1.4f, 0.0f, 0.45f), glm::vec3(2.8f, 0.4f, 0.9f), baseColor, root);   // base
    placeCube(cubeVAO, shader, glm::vec3(1.4f, 0.4f, 0.85f), glm::vec3(2.8f, 0.7f, 0.2f), baseColor, root);   // backrest
    placeCube(cubeVAO, shader, glm::vec3(2.65f, 0.4f, 0.45f), glm::vec3(0.3f, 0.55f, 0.9f), baseColor, root); // right armrest

    // 4 seat cushions along the main run
    for (int i = 0; i < 4; i++)
        placeCube(cubeVAO, shader, glm::vec3(0.35f + i * 0.7f, 0.4f, 0.42f), glm::vec3(0.66f, 0.16f, 0.82f), cushionColor, root);

    // decorative back pillows, alternating colors
    placeCube(cubeVAO, shader, glm::vec3(0.55f, 0.5f, 0.65f), glm::vec3(0.4f, 0.4f, 0.15f), pillowColorA, root);
    placeCube(cubeVAO, shader, glm::vec3(1.35f, 0.5f, 0.65f), glm::vec3(0.4f, 0.4f, 0.15f), pillowColorB, root);
    placeCube(cubeVAO, shader, glm::vec3(2.15f, 0.5f, 0.65f), glm::vec3(0.4f, 0.4f, 0.15f), pillowColorA, root);

    // ---- chaise (lower, no backrest, juts further into the room) ----
    placeCube(cubeVAO, shader, glm::vec3(-0.55f, 0.0f, 0.0f), glm::vec3(1.1f, 0.4f, 1.8f), baseColor, root);        // chaise base
    placeCube(cubeVAO, shader, glm::vec3(-0.55f, 0.4f, 0.05f), glm::vec3(1.0f, 0.16f, 1.55f), cushionColor, root);  // chaise cushion
    placeCube(cubeVAO, shader, glm::vec3(-0.55f, 0.40f, -0.85f), glm::vec3(1.05f, 0.32f, 0.25f), baseColor, root);  // raised end bolster
}

// ===========================================================
// Coffee table - local space, centered at local origin
// ===========================================================
void drawCoffeeTable(unsigned int& cubeVAO, Shader& shader, glm::mat4 root)
{
    glm::vec3 glassTop(0.75f, 0.78f, 0.80f);
    glm::vec3 metalLeg(0.12f, 0.12f, 0.12f);

    placeCube(cubeVAO, shader, glm::vec3(0.0f, 0.35f, 0.0f), glm::vec3(1.2f, 0.05f, 0.6f), glassTop, root);

    float lx = 0.5f, lz = 0.25f;
    for (int i = -1; i <= 1; i += 2)
        for (int j = -1; j <= 1; j += 2)
            placeCube(cubeVAO, shader, glm::vec3(i * lx, 0.0f, j * lz), glm::vec3(0.06f, 0.35f, 0.06f), metalLeg, root);
}

// ===========================================================
// Wall-mounted TV feature wall (right accent wall), centered
// on the wall: a walnut accent panel, a large flush-mounted
// screen, and a premium raised TV cabinet (legs, two-tone trim,
// 3-door look) below it.
// ===========================================================
void drawTVWall(unsigned int& cubeVAO, Shader& shader)
{
    glm::vec3 accentWood(0.35f, 0.22f, 0.12f);
    glm::vec3 tvBlack(0.03f, 0.03f, 0.03f);
    glm::vec3 cabinetWood(0.24f, 0.15f, 0.09f);
    glm::vec3 trimWood(0.55f, 0.42f, 0.25f);
    glm::vec3 darkSeam(0.05f, 0.05f, 0.05f);
    glm::vec3 handleMetal(0.65f, 0.65f, 0.68f);

    float wallZ = 0.0f; // centered on the wall

    // wood feature panel flush against the wall, larger to suit the bigger TV
    placeCube(cubeVAO, shader, glm::vec3(3.36f, 0.7f, wallZ), glm::vec3(0.05f, 2.0f, 2.6f), accentWood);

    // TV, mounted flush in front of the panel - bigger screen
    placeCube(cubeVAO, shader, glm::vec3(3.31f, 1.2f, wallZ), glm::vec3(0.05f, 1.15f, 2.15f), tvBlack);

    // ---- premium TV cabinet: raised on legs, two-tone, 3-door look ----
    float legH = 0.18f, bodyH = 0.35f;

    // 4 slim legs
    placeCube(cubeVAO, shader, glm::vec3(2.925f, 0.0f, wallZ - 1.2f), glm::vec3(0.04f, legH, 0.04f), handleMetal);
    placeCube(cubeVAO, shader, glm::vec3(3.375f, 0.0f, wallZ - 1.2f), glm::vec3(0.04f, legH, 0.04f), handleMetal);
    placeCube(cubeVAO, shader, glm::vec3(2.925f, 0.0f, wallZ + 1.2f), glm::vec3(0.04f, legH, 0.04f), handleMetal);
    placeCube(cubeVAO, shader, glm::vec3(3.375f, 0.0f, wallZ + 1.2f), glm::vec3(0.04f, legH, 0.04f), handleMetal);

    // cabinet body, raised above the legs
    placeCube(cubeVAO, shader, glm::vec3(3.15f, legH, wallZ), glm::vec3(0.55f, bodyH, 2.6f), cabinetWood);

    // lighter wood trim along the top edge (two-tone look)
    placeCube(cubeVAO, shader, glm::vec3(3.15f, legH + bodyH, wallZ), glm::vec3(0.58f, 0.02f, 2.64f), trimWood);

    // two vertical seams dividing the front into 3 "doors"
    float doorFrontX = 2.875f;
    float seamY = legH + bodyH * 0.5f;
    placeCube(cubeVAO, shader, glm::vec3(doorFrontX, seamY, wallZ - 0.43f), glm::vec3(0.02f, bodyH - 0.05f, 0.02f), darkSeam);
    placeCube(cubeVAO, shader, glm::vec3(doorFrontX, seamY, wallZ + 0.43f), glm::vec3(0.02f, bodyH - 0.05f, 0.02f), darkSeam);

    // a handle centered on each of the 3 doors
    for (int i = -1; i <= 1; i++)
        placeCube(cubeVAO, shader, glm::vec3(doorFrontX, seamY, wallZ + i * 0.867f), glm::vec3(0.02f, 0.10f, 0.02f), handleMetal);

    // small soundbar sitting on top of the cabinet
    placeCube(cubeVAO, shader, glm::vec3(3.0f, legH + bodyH + 0.02f, wallZ), glm::vec3(0.15f, 0.08f, 0.8f), darkSeam);
}

// ===========================================================
// Floor lamp - this is where the SPOTLIGHT is mounted
// ===========================================================
void drawFloorLamp(unsigned int& cubeVAO, Shader& shader, glm::vec3 basePos)
{
    glm::vec3 brass(0.72f, 0.60f, 0.20f);
    glm::vec3 shade(0.92f, 0.86f, 0.76f);

    placeCube(cubeVAO, shader, basePos, glm::vec3(0.08f, 1.6f, 0.08f), brass);
    placeCube(cubeVAO, shader, glm::vec3(basePos.x, basePos.y + 1.6f, basePos.z), glm::vec3(0.4f, 0.3f, 0.4f), shade);
}

// ===========================================================
// Side table + table lamp - this is where POINT LIGHT 2 is
// ===========================================================
void drawSideTableWithLamp(unsigned int& cubeVAO, Shader& shader, glm::vec3 pos)
{
    glm::vec3 wood(0.30f, 0.18f, 0.10f);
    glm::vec3 brass(0.72f, 0.60f, 0.20f);
    glm::vec3 shade(0.95f, 0.85f, 0.65f);

    placeCube(cubeVAO, shader, pos, glm::vec3(0.4f, 0.45f, 0.4f), wood);
    placeCube(cubeVAO, shader, glm::vec3(pos.x, pos.y + 0.45f, pos.z), glm::vec3(0.06f, 0.25f, 0.06f), brass);
    placeCube(cubeVAO, shader, glm::vec3(pos.x, pos.y + 0.70f, pos.z), glm::vec3(0.28f, 0.18f, 0.28f), shade);
}

// ===========================================================
// Potted plant - pot + layered box "foliage" (cubes only)
// ===========================================================
void drawPlant(unsigned int& cubeVAO, Shader& shader, glm::vec3 pos)
{
    glm::vec3 leafDark(0.10f, 0.35f, 0.12f);
    glm::vec3 leafLight(0.16f, 0.45f, 0.18f);

    // pot
    placeCube(cubeVAO, shader, pos, glm::vec3(0.35f, 0.35f, 0.35f), glm::vec3(0.45f, 0.25f, 0.15f));

    // stacked, slightly rotated boxes stand in for a leafy bush
    float potTop = pos.y + 0.35f;
    placeCube(cubeVAO, shader, glm::vec3(pos.x, potTop, pos.z), glm::vec3(0.42f, 0.32f, 0.42f), leafDark);

    glm::mat4 root = glm::translate(glm::mat4(1.0f), glm::vec3(pos.x, 0.0f, pos.z));
    root = glm::rotate(root, glm::radians(35.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    placeCube(cubeVAO, shader, glm::vec3(0.0f, potTop + 0.24f, 0.0f), glm::vec3(0.30f, 0.26f, 0.30f), leafLight, root);
    placeCube(cubeVAO, shader, glm::vec3(0.0f, potTop + 0.42f, 0.0f), glm::vec3(0.18f, 0.20f, 0.18f), leafDark, root);
}

// ===========================================================
// Dining table - simple rectangular top on 4 legs
// ===========================================================
void drawDiningTable(unsigned int& cubeVAO, Shader& shader, glm::mat4 root)
{
    glm::vec3 tabletop(0.90f, 0.88f, 0.83f); // whitish/cream top

    placeCube(cubeVAO, shader, glm::vec3(0.0f, 0.75f, 0.0f), glm::vec3(1.4f, 0.06f, 0.9f), tabletop, root);

    float lx = 0.62f, lz = 0.37f;
    for (int i = -1; i <= 1; i += 2)
        for (int j = -1; j <= 1; j += 2)
            placeCube(cubeVAO, shader, glm::vec3(i * lx, 0.0f, j * lz), glm::vec3(0.06f, 0.75f, 0.06f), glm::vec3(0.12f, 0.12f, 0.12f), root);
}

// ===========================================================
// Simple dining chair - seat + backrest + 4 legs.
// Faces +Z by default (backrest on the -Z side); rotate with
// the root to face a different way.
// ===========================================================
void drawDiningChair(unsigned int& cubeVAO, Shader& shader, glm::mat4 root)
{
    glm::vec3 wood(0.28f, 0.17f, 0.10f);

    placeCube(cubeVAO, shader, glm::vec3(0.0f, 0.45f, 0.0f), glm::vec3(0.42f, 0.05f, 0.42f), wood, root);       // seat
    placeCube(cubeVAO, shader, glm::vec3(0.0f, 0.45f, -0.19f), glm::vec3(0.42f, 0.5f, 0.05f), wood, root);      // backrest

    float lx = 0.17f, lz = 0.17f;
    for (int i = -1; i <= 1; i += 2)
        for (int j = -1; j <= 1; j += 2)
            placeCube(cubeVAO, shader, glm::vec3(i * lx, 0.0f, j * lz), glm::vec3(0.05f, 0.45f, 0.05f), wood, root);
}

// ===========================================================
// Open floating WALL bookshelf (mounted flush on the wall, no
// enclosing cabinet) with rows of colorful books + a vase
// ===========================================================
void drawBookshelf(unsigned int& cubeVAO, Shader& shader, glm::vec3 pos)
{
    glm::vec3 wood(0.30f, 0.18f, 0.10f);
    glm::vec3 bookColors[6] = {
        glm::vec3(0.75f, 0.20f, 0.20f), glm::vec3(0.20f, 0.45f, 0.65f),
        glm::vec3(0.85f, 0.65f, 0.20f), glm::vec3(0.35f, 0.55f, 0.30f),
        glm::vec3(0.55f, 0.30f, 0.60f), glm::vec3(0.80f, 0.45f, 0.15f)
    };

    // pos = (x against the wall, 0, z center of the shelf run)
    float shelfHeights[3] = { 0.65f, 1.15f, 1.65f };

    for (int s = 0; s < 3; s++)
    {
        float shelfY = shelfHeights[s];

        // the floating shelf plank itself, flush against the wall
        placeCube(cubeVAO, shader, glm::vec3(pos.x, shelfY, pos.z), glm::vec3(0.28f, 0.04f, 1.0f), wood);

        float shelfTop = shelfY + 0.04f;

        if (s < 2)
        {
            // rows of standing books along the shelf
            for (int i = 0; i < 5; i++)
            {
                glm::vec3 c = bookColors[(s * 5 + i) % 6];
                float h = 0.20f + 0.02f * ((i + s) % 3);
                placeCube(cubeVAO, shader,
                    glm::vec3(pos.x + 0.12f, shelfTop, pos.z - 0.4f + i * 0.2f),
                    glm::vec3(0.16f, h, 0.16f), c);
            }
        }
        else
        {
            // top shelf: a few books lying flat + a small box "vase"
            placeCube(cubeVAO, shader, glm::vec3(pos.x + 0.10f, shelfTop, pos.z - 0.25f), glm::vec3(0.20f, 0.05f, 0.15f), bookColors[1]);
            placeCube(cubeVAO, shader, glm::vec3(pos.x + 0.10f, shelfTop + 0.05f, pos.z - 0.25f), glm::vec3(0.18f, 0.05f, 0.14f), bookColors[4]);

            placeCube(cubeVAO, shader, glm::vec3(pos.x + 0.12f, shelfTop, pos.z + 0.25f), glm::vec3(0.10f, 0.22f, 0.10f), glm::vec3(0.65f, 0.20f, 0.25f));
        }
    }
}

// ===========================================================
// Ceiling fan - a required MOVING OBJECT (4 rotating blades)
// ===========================================================
void drawCeilingFan(unsigned int& cubeVAO, Shader& shader, float angle)
{
    float hubY = 2.82f; // height of the hub's vertical CENTER - blades pivot here
    glm::vec3 metal(0.15f, 0.15f, 0.15f);
    glm::vec3 bladeColor(0.22f, 0.13f, 0.08f);

    // mounting rod from the ceiling down to the top of the hub
    placeCube(cubeVAO, shader, glm::vec3(0.0f, hubY + 0.07f, 0.0f), glm::vec3(0.05f, 0.13f, 0.05f), metal);

    // hub / motor housing, centered at hubY so the blades pass through its middle
    placeCube(cubeVAO, shader, glm::vec3(0.0f, hubY - 0.07f, 0.0f), glm::vec3(0.22f, 0.14f, 0.22f), metal);

    // 4 blades, spinning around the vertical (Y) axis through the hub's center
    for (int i = 0; i < 4; i++)
    {
        glm::mat4 root = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, hubY, 0.0f));
        root = glm::rotate(root, glm::radians(angle + i * 90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        placeCube(cubeVAO, shader, glm::vec3(0.5f, -0.015f, 0.0f), glm::vec3(0.85f, 0.03f, 0.16f), bladeColor, root);
    }
}

// ===========================================================
// Pendant fixture over the coffee table: cord + brass shade,
// dressing up POINT LIGHT 1 (its bright marker cube is drawn
// separately, unlit, in the render loop)
// ===========================================================
void drawPendantFixture(unsigned int& cubeVAO, Shader& shader, glm::vec3 lightPos)
{
    placeCube(cubeVAO, shader, glm::vec3(lightPos.x, lightPos.y + 0.12f, lightPos.z), glm::vec3(0.02f, 0.13f, 0.02f), glm::vec3(0.10f, 0.10f, 0.10f));

    // flattened box shade (brass) instead of a sphere
    placeCube(cubeVAO, shader, glm::vec3(lightPos.x, lightPos.y + 0.08f, lightPos.z), glm::vec3(0.34f, 0.09f, 0.34f), glm::vec3(0.55f, 0.45f, 0.20f));
}

int main()
{
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    // --------------------
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "CSE 444: Premium Living Room", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync - smooths out movement/animation pacing
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // capture the mouse for FPS-style look-around
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    // build and compile shaders
    // --------------------------
    Shader lightingShader("vertexShaderForPhongShading.vs", "fragmentShaderForPhongShading.fs");
    Shader ourShader("vertexShader.vs", "fragmentShader.fs"); // unlit shader, used for light markers

    // cube geometry (positions + normals)
    // ------------------------------------
    float cube_vertices[] = {
        // positions      // normals
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f,
        1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f,
        1.0f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f,
        0.0f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f,

        1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
        1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f,
        1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f,

        0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
        0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,

        0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f,

        1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f,
        1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f,

        0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f,
        1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f,
        1.0f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f
    };
    unsigned int cube_indices[] = {
        0, 3, 2, 2, 1, 0,
        4, 5, 7, 7, 6, 4,
        8, 9, 10, 10, 11, 8,
        12, 13, 14, 14, 15, 12,
        16, 17, 18, 18, 19, 16,
        20, 21, 22, 22, 23, 20
    };

    unsigned int cubeVAO, cubeVBO, cubeEBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glGenBuffers(1, &cubeEBO);

    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube_vertices), cube_vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cube_indices), cube_indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)12);
    glEnableVertexAttribArray(1);

    // a second VAO for the (unlit) light-marker cubes, sharing the same VBO/EBO
    unsigned int lightCubeVAO;
    glGenVertexArrays(1, &lightCubeVAO);
    glBindVertexArray(lightCubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // furniture group transforms (world placement for local-space builders)
    // single L-shaped sectional against the back wall; chaise extends
    // toward the left (beige) wall, main run + armrest toward the right.
    // Pulled further left so the main run stays clear of the sliding door.
    // base light values, captured once - lightIntensity scales from these each frame
    glm::vec3 basePL1Amb = pointlight1.ambient, basePL1Diff = pointlight1.diffuse;
    glm::vec3 basePL2Amb = pointlight2.ambient, basePL2Diff = pointlight2.diffuse;
    glm::vec3 baseSpotAmb = spotlight.ambient, baseSpotDiff = spotlight.diffuse;

    glm::mat4 sectionalRoot = glm::translate(glm::mat4(1.0f), glm::vec3(-2.0f, 0.0f, 2.0f));
    glm::mat4 tableRoot = glm::translate(glm::mat4(1.0f), glm::vec3(-0.6f, 0.0f, 1.3f));

    // dining set, in the open floor area beside the bookshelf - the
    // table's long side sits flush against the left (beige) wall
    glm::mat4 diningTableRoot = glm::rotate(
        glm::translate(glm::mat4(1.0f), glm::vec3(-2.93f, 0.0f, -1.9f)),
        glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    // 2 chairs along the open (room-facing) long side, facing the table
    glm::mat4 diningChairA = glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(-2.0f, 0.0f, -2.3f)), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 diningChairB = glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(-2.0f, 0.0f, -1.5f)), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    // 2 chairs at the short ends, facing inward
    glm::mat4 diningChairC = glm::translate(glm::mat4(1.0f), glm::vec3(-2.93f, 0.0f, -3.0f));
    glm::mat4 diningChairD = glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(-2.93f, 0.0f, -0.8f)), glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    // render loop
    // -----------
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        if (deltaTime > 0.1f) deltaTime = 0.1f; // avoid huge jumps (e.g. after a breakpoint)

        processInput(window);

        // ---- update animations ----
        // fan gradually speeds up to max, and gradually slows down when turned off
        float fanTargetSpeed = fanSpinning ? fanMaxSpeed : 0.0f;
        if (fanSpeed < fanTargetSpeed)
        {
            fanSpeed += fanAccel * deltaTime;
            if (fanSpeed > fanTargetSpeed) fanSpeed = fanTargetSpeed;
        }
        else if (fanSpeed > fanTargetSpeed)
        {
            fanSpeed -= fanAccel * deltaTime;
            if (fanSpeed < fanTargetSpeed) fanSpeed = fanTargetSpeed;
        }
        fanAngle += fanSpeed * deltaTime;
        if (fanAngle > 360.0f) fanAngle -= 360.0f;

        float slideTarget = doorOpen ? 1.15f : 0.0f;
        float t = 4.0f * deltaTime;
        if (t > 1.0f) t = 1.0f;
        doorSlide += (slideTarget - doorSlide) * t;

        // render
        // ------
        glClearColor(0.10f, 0.10f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        lightingShader.use();
        lightingShader.setVec3("viewPos", camera.Position);

        // clamp and apply the overall brightness multiplier
        if (lightIntensity < 0.2f) lightIntensity = 0.2f;
        if (lightIntensity > 2.5f) lightIntensity = 2.5f;
        pointlight1.ambient = basePL1Amb * lightIntensity;
        pointlight1.diffuse = basePL1Diff * lightIntensity;
        pointlight2.ambient = basePL2Amb * lightIntensity;
        pointlight2.diffuse = basePL2Diff * lightIntensity;
        spotlight.ambient = baseSpotAmb * lightIntensity;
        spotlight.diffuse = baseSpotDiff * lightIntensity;

        pointlight1.setUpPointLight(lightingShader);
        pointlight2.setUpPointLight(lightingShader);
        spotlight.setUpSpotLight(lightingShader);

        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        lightingShader.setMat4("projection", projection);

        glm::mat4 view = camera.GetViewMatrix();
        lightingShader.setMat4("view", view);

        // ---- scene ----
        drawRoom(cubeVAO, lightingShader);
        drawTrim(cubeVAO, lightingShader);
        drawSlidingDoor(cubeVAO, lightingShader, doorSlide);
        drawSectionalSofa(cubeVAO, lightingShader, sectionalRoot);
        drawCoffeeTable(cubeVAO, lightingShader, tableRoot);
        drawTVWall(cubeVAO, lightingShader);
        drawFloorLamp(cubeVAO, lightingShader, glm::vec3(-2.55f, 0.0f, 0.7f));
        drawSideTableWithLamp(cubeVAO, lightingShader, glm::vec3(1.3f, 0.0f, 2.5f));
        drawPlant(cubeVAO, lightingShader, glm::vec3(2.4f, 0.0f, -2.4f));

        // dining set, in the open floor area beside the bookshelf
        drawDiningTable(cubeVAO, lightingShader, diningTableRoot);
        drawDiningChair(cubeVAO, lightingShader, diningChairA);
        drawDiningChair(cubeVAO, lightingShader, diningChairB);
        drawDiningChair(cubeVAO, lightingShader, diningChairC);
        drawDiningChair(cubeVAO, lightingShader, diningChairD);

        drawBookshelf(cubeVAO, lightingShader, glm::vec3(-3.24f, 0.0f, 0.3f));
        drawCeilingFan(cubeVAO, lightingShader, fanAngle);
        drawPendantFixture(cubeVAO, lightingShader, pointlight1.position);

        // rug (flat colored box) - sized to sit under the coffee table and the sectional
        placeCube(cubeVAO, lightingShader, glm::vec3(-0.6f, 0.005f, 1.8f), glm::vec3(4.0f, 0.02f, 2.4f), glm::vec3(0.65f, 0.30f, 0.20f));

        // ---- unlit markers showing exactly where each light source is ----
        ourShader.use();
        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);
        glBindVertexArray(lightCubeVAO);

        glm::mat4 markerModel;

        markerModel = glm::translate(glm::mat4(1.0f), pointlight1.position);
        markerModel = glm::scale(markerModel, glm::vec3(0.14f));
        ourShader.setMat4("model", markerModel);
        ourShader.setVec3("color", pointLightsOn ? glm::vec3(1.0f, 0.9f, 0.6f) : glm::vec3(0.3f, 0.3f, 0.3f));
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

        markerModel = glm::translate(glm::mat4(1.0f), pointlight2.position);
        markerModel = glm::scale(markerModel, glm::vec3(0.14f));
        ourShader.setMat4("model", markerModel);
        ourShader.setVec3("color", pointLightsOn ? glm::vec3(1.0f, 0.7f, 0.3f) : glm::vec3(0.3f, 0.3f, 0.3f));
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

        markerModel = glm::translate(glm::mat4(1.0f), spotlight.position);
        markerModel = glm::scale(markerModel, glm::vec3(0.13f));
        ourShader.setMat4("model", markerModel);
        ourShader.setVec3("color", spotLightOn ? glm::vec3(0.7f, 0.8f, 1.0f) : glm::vec3(0.3f, 0.3f, 0.3f));
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteVertexArrays(1, &lightCubeVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteBuffers(1, &cubeEBO);

    glfwTerminate();
    return 0;
}

// ---------------------------------------------------------
// keyboard movement + toggles polled every frame
// ---------------------------------------------------------
void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        camera.Position.y += 2.0f * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
        camera.Position.y -= 2.0f * deltaTime;

    // Arrow keys also turn the camera (in addition to mouse-look) -
    // useful if mouse capture isn't behaving on a particular machine.
    float turnSpeed = 60.0f; // degrees per second
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
        camera.ProcessMouseMovement(-turnSpeed * deltaTime / camera.MouseSensitivity, 0.0f);
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        camera.ProcessMouseMovement(turnSpeed * deltaTime / camera.MouseSensitivity, 0.0f);
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
        camera.ProcessMouseMovement(0.0f, turnSpeed * deltaTime / camera.MouseSensitivity);
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
        camera.ProcessMouseMovement(0.0f, -turnSpeed * deltaTime / camera.MouseSensitivity);

    // Z / X zoom in/out (mouse scroll wheel does the same thing, via scroll_callback)
    if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS)
        camera.ProcessMouseScroll(30.0f * deltaTime);
    if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS)
        camera.ProcessMouseScroll(-30.0f * deltaTime);

    // = / - raise or lower the brightness of every light in the room together
    if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS)
        lightIntensity += 0.6f * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS)
        lightIntensity -= 0.6f * deltaTime;
}

// ---------------------------------------------------------
// edge-triggered key presses: light toggles, fan/door toggles
// ---------------------------------------------------------
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action != GLFW_PRESS) return;

    if (key == GLFW_KEY_1)
    {
        pointLightsOn = !pointLightsOn;
        if (pointLightsOn) { pointlight1.turnOn();  pointlight2.turnOn(); }
        else { pointlight1.turnOff(); pointlight2.turnOff(); }
    }

    if (key == GLFW_KEY_2)
    {
        spotLightOn = !spotLightOn;
        if (spotLightOn) spotlight.turnOn();
        else              spotlight.turnOff();
    }

    if (key == GLFW_KEY_F)
    {
        fanSpinning = !fanSpinning;
    }

    if (key == GLFW_KEY_G)
    {
        doorOpen = !doorOpen;
    }
}

// ---------------------------------------------------------
// mouse look
// ---------------------------------------------------------
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

// ---------------------------------------------------------
// mouse scroll wheel - zoom in/out (adjusts the camera's FOV)
// ---------------------------------------------------------
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

// ---------------------------------------------------------
// window resize
// ---------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}