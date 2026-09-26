/*
 * glut-demo - a GLUT program, ported by compiling it.
 *
 * It shows that a program written against GLUT builds against this SDK: main,
 * glutInit, callbacks, glutMainLoop, GLU matrices and mipmaps, the GLUT solids
 * (including the teapot, drawn through glMap2f/glEvalMesh2), window calls that change
 * nothing here, and a glutBitmapString HUD.
 *
 * Everything above the entry point is portable: no SDK call, header or platform name,
 * so it compiles in a desktop GLUT project. The last lines are the payload entry point,
 * which calls main because a homebrew payload is entered by name.
 */
#include <GL/glut.h>
/* The headers a port's own code includes. They resolve to oops-sdk's include/libc on
 * the target and to the real ones on a desktop. */
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static float g_angle = 0.0f;
static float g_spin = 0.6f;
static GLuint g_texture = 0;
static int g_frames = 0;

/* A checkerboard handed to gluBuild2DMipmaps, as pre-GL_GENERATE_MIPMAP code does.
 * 64x64 so the chain halves cleanly to 1x1. */
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
    gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE,
                      pixels);
}

/*
 * A settings file read the way a port reads one: fopen, fgets, a <ctype.h> parser,
 * fclose. No file ships at this path, so it runs the not-found path.
 */
static float read_setting(const char *path, const char *key, float fallback) {
    FILE *f = fopen(path, "r");
    if (!f) {
        printf("glut-demo: no %s, using %s=%d/100\n", path, key,
               (int)(fallback * 100.0f));
        return fallback;
    }
    char line[128];
    float value = fallback;
    while (fgets(line, (int)sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq)
            continue;
        *eq = '\0';
        char *name = line;
        while (isspace((int)(unsigned char)*name))
            name++;
        if (strcmp(name, key) == 0)
            value = (float)atof(eq + 1);
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
    if (height < 1)
        height = 1;
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

    /* A dodecahedron opposite it. Its pentagons are lit flat, one normal each, and a
     * face with the wrong winding shows as a hole. */
    glPushMatrix();
    glRotatef(-g_angle * 0.8f + 180.0f, 0.0f, 1.0f, 0.0f);
    glTranslatef(3.4f, 0.0f, 0.0f);
    glRotatef(-g_angle * 1.1f, 0.0f, 1.0f, 0.3f);
    glScalef(0.42f, 0.42f, 0.42f);
    glColor3f(0.45f, 0.55f, 0.95f);
    glutSolidDodecahedron();
    glPopMatrix();

    /* The teapot, a quarter turn round from the cube: Bezier patches evaluated through
     * glMap2f/glEvalMesh2, the one shape here that exercises the evaluator. GLUT's size
     * argument is a scale factor, so glutSolidTeapot(0.5) is about 1.6 units across.
     * It spins about its own axis so the handle and spout stay readable. */
    glPushMatrix();
    glRotatef(-g_angle * 0.8f + 90.0f, 0.0f, 1.0f, 0.0f);
    glTranslatef(3.4f, 0.0f, 0.0f);
    glRotatef(g_angle * 0.9f, 0.0f, 1.0f, 0.0f);
    glColor3f(0.95f, 0.72f, 0.25f);
    glutSolidTeapot(0.5);
    glPopMatrix();

    /* A frame counter in the GLUT bitmap font, the ordinary recipe: window coordinates,
     * lighting and depth off, and the state put back. */
    {
        const int w = glutGet(GLUT_WINDOW_WIDTH), h = glutGet(GLUT_WINDOW_HEIGHT);
        char hud[48];
        snprintf(hud, sizeof(hud), "frames %d  %d x %d  q or escape to quit", g_frames,
                 w, h);
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

/* A wobble from <math.h> on a clock read through GLUT. */
static float wobble(float t) {
    const float a = sinf(t * 0.7f), b = cosf(t * 1.3f);
    return 0.15f * sqrtf(fabsf(a * b)) + 0.02f * (float)(rand() % 16) / 16.0f;
}

static void idle(void) {
    const float t = (float)glutGet(GLUT_ELAPSED_TIME) * 0.001f;
    g_angle += g_spin + wobble(t);
    if (g_angle >= 360.0f)
        g_angle -= 360.0f;
    glutPostRedisplay();
}

static void keyboard(unsigned char key, int x, int y) {
    (void)x;
    (void)y;
    /* Escape leaves; on a console the Options button sends it (glutOopsPadKeys). */
    if (key == 27 || key == 'q')
        glutLeaveMainLoop();
}

static void special(int key, int x, int y) {
    (void)x;
    (void)y;
    if (key == GLUT_KEY_LEFT)
        g_angle -= 5.0f;
    if (key == GLUT_KEY_RIGHT)
        g_angle += 5.0f;
    glutPostRedisplay();
}

int main(int argc, char **argv) {
    /* Allocated and copied the way a port loads its own data: malloc, strcpy, strlen,
     * free. */
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
    /* Calls a real program makes that change nothing here: the title has nowhere to
     * go, the window is already full screen, and there is no cursor. */
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
 * The payload entry point: a payload is entered by name, not by the C runtime.
 * --------------------------------------------------------------------------- */
#ifndef OOPS_HOST_BUILD
#include "oops/syscall.h"

int glut_demo_start(const payload_args_t *args);

__attribute__((visibility("default"))) int glut_demo_start(const payload_args_t *args) {
    if (args)
        sys_call_init(args);
    return main(0, (char **)0);
}
#endif
