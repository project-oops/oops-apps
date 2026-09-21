/*
 * glut-demo - a GLUT program, ported by compiling it.
 *
 * This exists to be evidence rather than a demonstration. oops-sdk grew `<GL/glut.h>` and the
 * rest of GLU on 2026-09-20, and the claim that made was that a program written against GLUT
 * builds against this SDK. The only way to know is to write one the way that code is actually
 * written - `main`, `glutInit`, callbacks, `glutMainLoop`, the quadric solids, a mipmapped
 * texture built by `gluBuild2DMipmaps` - and build it for the console.
 *
 * **Everything below the entry point is portable.** There is no SDK call in it, no header of
 * this SDK's, nothing that names the platform. Copy it into a desktop GLUT project and it
 * compiles there; that is the point. The only platform-specific lines are the last ones, where
 * a payload entry point calls `main` - because a homebrew payload is entered by name and not by
 * the C runtime.
 *
 * What it exercises, chosen to be the parts a port actually leans on:
 *
 * - the window, the callbacks, the main loop, and leaving it;
 * - `gluPerspective` and `gluLookAt` on the matrix stack;
 * - `gluBuild2DMipmaps`, the way every texture loader written before GL 1.4 built its chain;
 * - `glutSolidSphere`, `glutSolidTorus` and `glutSolidCube`, the shapes that code draws, and
 *   `glutSolidDodecahedron`, whose flat faces show a winding mistake as a hole;
 * - `glutSolidTeapot`, which is none of the above: it is the only GLUT solid that is measured
 *   data rather than arithmetic, and it is drawn through the **evaluator** - so it is the one
 *   shape here that exercises `glMap2f`/`glEvalMesh2` instead of the quadric path;
 * - `glutSetWindowTitle`, `glutFullScreen` and `glutSetCursor`, which change nothing here and
 *   which a port calls anyway;
 * - `glutBitmapCharacter` through `glutBitmapString`, drawing a HUD the way every GLUT program
 *   draws one: an ortho push, lighting and depth off, and the state put back;
 * - lighting, depth, and a redisplay driven from an idle callback.
 */
#include <GL/glut.h>
/* The headers a port's own code includes without thinking about it. They resolve to oops-sdk's
 * `include/libc`, which is on the target include path; on a desktop they are the real ones. */
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static float g_angle = 0.0f;
static float g_spin = 0.6f;
static GLuint g_texture = 0;
static int g_frames = 0;

/* A checkerboard, built once and handed to gluBuild2DMipmaps - which is how this was done before
 * GL_GENERATE_MIPMAP existed, and so how the code being ported does it. 64x64 is not a power of
 * two by accident: the chain has to halve cleanly to 1x1. */
static void make_texture(void) {
    static GLubyte pixels[64][64][4];
    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 64; x++) {
            const int on = ((x >> 3) + (y >> 3)) & 1;
            pixels[y][x][0] = (GLubyte)(on ? 230 : 40);
            pixels[y][x][1] = (GLubyte)(on ? 180 : 60);
            pixels[y][x][2] = (GLubyte)(on ? 60 : 160);
            pixels[y][x][3] = 255;
        }
    }
    glGenTextures(1, &g_texture);
    glBindTexture(GL_TEXTURE_2D, g_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
}

/*
 * A settings file read the way a port reads one: fopen, fgets, a parser out of <ctype.h>, fclose.
 * There is no file at this path and that is fine - the point is that a port's loader compiles,
 * links and runs its not-found path rather than faulting on a missing `fopen`.
 */
static float read_setting(const char *path, const char *key, float fallback) {
    FILE *f = fopen(path, "r");
    if (!f) {
        printf("glut-demo: no %s, using %s=%d/100\n", path, key, (int)(fallback * 100.0f));
        return fallback;
    }
    char line[128];
    float value = fallback;
    while (fgets(line, (int)sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *name = line;
        while (isspace((int)(unsigned char)*name)) name++;
        if (strcmp(name, key) == 0) value = (float)atof(eq + 1);
    }
    fclose(f);
    return value;
}

static void init_scene(void) {
    static const GLfloat light_pos[4] = {2.0f, 3.0f, 4.0f, 1.0f};
    static const GLfloat light_col[4] = {1.0f, 1.0f, 0.95f, 1.0f};
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_col);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glClearColor(0.06f, 0.07f, 0.10f, 1.0f);
    make_texture();
}

static void reshape(int width, int height) {
    if (height < 1) height = 1;
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(55.0, (double)width / (double)height, 0.5, 60.0);
    glMatrixMode(GL_MODELVIEW);
}

static void display(void) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    gluLookAt(0.0, 1.6, 7.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);

    /* A textured sphere in the middle. */
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_texture);
    glPushMatrix();
    glRotatef(g_angle, 0.2f, 1.0f, 0.0f);
    glColor3f(1.0f, 1.0f, 1.0f);
    glutSolidSphere(1.4, 32, 24);
    glPopMatrix();
    glDisable(GL_TEXTURE_2D);

    /* A torus around it, and a small cube orbiting. */
    glPushMatrix();
    glRotatef(g_angle * 0.4f, 1.0f, 0.0f, 0.0f);
    glColor3f(0.9f, 0.4f, 0.3f);
    glutSolidTorus(0.25, 2.6, 16, 40);
    glPopMatrix();

    glPushMatrix();
    glRotatef(-g_angle * 0.8f, 0.0f, 1.0f, 0.0f);
    glTranslatef(3.4f, 0.0f, 0.0f);
    glRotatef(g_angle * 1.7f, 1.0f, 1.0f, 0.0f);
    glColor3f(0.4f, 0.8f, 0.5f);
    glutSolidCube(0.8);
    glPopMatrix();

    /* And a dodecahedron opposite it. Its twelve pentagons are lit flat, which is what says the
     * faces came out with one normal each and the right winding - a face wound the other way
     * disappears under the cull and leaves a hole in the solid. */
    glPushMatrix();
    glRotatef(-g_angle * 0.8f + 180.0f, 0.0f, 1.0f, 0.0f);
    glTranslatef(3.4f, 0.0f, 0.0f);
    glRotatef(-g_angle * 1.1f, 0.0f, 1.0f, 0.3f);
    glScalef(0.42f, 0.42f, 0.42f);
    glColor3f(0.45f, 0.55f, 0.95f);
    glutSolidDodecahedron();
    glPopMatrix();

    /* And the teapot, a quarter turn round from the cube.
     *
     * It is here because it is the one solid in GLUT that is not arithmetic. Everything above
     * comes out of a formula - a quadric, or the vertices of a platonic solid - while the teapot
     * is 129 control points Martin Newell measured off his own in 1975, drawn as ten Bezier
     * patches mirrored into thirty-two and evaluated through `glMap2f`/`glEvalMesh2`. So it is
     * the only shape on screen that exercises the evaluator, and the only one whose silhouette
     * would be wrong if the patch maths were.
     *
     * `glutSolidTeapot(0.5)` is about 1.6 units across, not 0.5: GLUT's size argument is a scale
     * factor rather than an extent, and matching that is the point - a ported program expects
     * GLUT's proportions, not tidier ones. It spins about its own axis only, so the handle and
     * spout stay readable as they come round.
     */
    glPushMatrix();
    glRotatef(-g_angle * 0.8f + 90.0f, 0.0f, 1.0f, 0.0f);
    glTranslatef(3.4f, 0.0f, 0.0f);
    glRotatef(g_angle * 0.9f, 0.0f, 1.0f, 0.0f);
    glColor3f(0.95f, 0.72f, 0.25f);
    glutSolidTeapot(0.5);
    glPopMatrix();

    /* **A frame counter drawn with glutBitmapCharacter**, which is what most GLUT code uses the
     * font for. Text goes in window coordinates with lighting and depth off, and the state is
     * put back - the ordinary recipe, and the reason it is here is that a port writes exactly
     * this and until 2026-09-20 it would not have linked. */
    {
        const int w = glutGet(GLUT_WINDOW_WIDTH), h = glutGet(GLUT_WINDOW_HEIGHT);
        char hud[48];
        snprintf(hud, sizeof(hud), "frames %d  %d x %d  q or escape to quit", g_frames, w, h);
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0.0, (double)w, 0.0, (double)h, -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        glDisable(GL_LIGHTING);
        glDisable(GL_DEPTH_TEST);
        glColor3f(1.0f, 1.0f, 1.0f);
        glRasterPos2i(12, h - glutBitmapHeight(GLUT_BITMAP_9_BY_15) - 8);
        glutBitmapString(GLUT_BITMAP_9_BY_15, (const unsigned char *)hud);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_LIGHTING);
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
    }

    glutSwapBuffers();
    g_frames++;
}

/* A wobble computed the way a port computes one: `sinf` and `sqrtf` out of <math.h>, on a clock
 * read through GLUT. Nothing here needs the SDK to be named. */
static float wobble(float t) {
    const float a = sinf(t * 0.7f), b = cosf(t * 1.3f);
    return 0.15f * sqrtf(fabsf(a * b)) + 0.02f * (float)(rand() % 16) / 16.0f;
}

static void idle(void) {
    const float t = (float)glutGet(GLUT_ELAPSED_TIME) * 0.001f;
    g_angle += g_spin + wobble(t);
    if (g_angle >= 360.0f) g_angle -= 360.0f;
    glutPostRedisplay();
}

static void keyboard(unsigned char key, int x, int y) {
    (void)x;
    (void)y;
    /* Escape leaves, which on a console is the option button as well - see glutOopsPadKeys. */
    if (key == 27 || key == 'q') glutLeaveMainLoop();
}

static void special(int key, int x, int y) {
    (void)x;
    (void)y;
    if (key == GLUT_KEY_LEFT) g_angle -= 5.0f;
    if (key == GLUT_KEY_RIGHT) g_angle += 5.0f;
    glutPostRedisplay();
}

int main(int argc, char **argv) {
    /* Allocated and copied the way a port loads its own data: malloc, strcpy, strlen, free. */
    char *name = (char *)malloc(32);
    if (name) {
        strcpy(name, "glut-demo");
        if (strlen(name) != 9u || strcmp(name, "glut-demo") != 0) {
            free(name);
            return 1;
        }
        free(name);
    }
    srand(1u);
    g_spin = read_setting("/app0/glut-demo.cfg", "spin", 0.6f);
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(1280, 720);
    glutCreateWindow("glut-demo");
    /* What a port does next, and what this SDK does with it: the title has nowhere to go and is
     * accepted, the window is already full screen, and the cursor is already none. Each call is
     * here because a real program makes it, not because it changes anything. */
    glutSetWindowTitle("glut-demo - oops-glut");
    glutFullScreen();
    glutSetCursor(GLUT_CURSOR_NONE);
    init_scene();
    reshape(glutGet(GLUT_WINDOW_WIDTH), glutGet(GLUT_WINDOW_HEIGHT));
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutIdleFunc(idle);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutMainLoop();
    return 0;
}

/* ---------------------------------------------------------------------------
 * The only part that is not portable: a payload is entered by name, not by the C runtime.
 * --------------------------------------------------------------------------- */
#ifndef OOPS_HOST_BUILD
#include "oops/syscall.h"

int glut_demo_start(const payload_args_t *args);

__attribute__((visibility("default"))) int glut_demo_start(const payload_args_t *args) {
    if (args) sys_call_init(args);
    return main(0, (char **)0);
}
#endif
