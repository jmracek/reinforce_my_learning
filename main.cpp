#include<string>
#include<chrono>
#include<iostream>
#include<GL/glew.h>
#include<glm/glm.hpp>
#include<GLFW/glfw3.h>

constexpr float RADIUS = 0.75;
constexpr float FRAMERATE = 120.0f;
constexpr float SECONDS_BETWEEN_FRAMES = 1 / FRAMERATE;

constexpr float VELOCITY_X = 0.25; // screen units per second
constexpr float VELOCITY_Y = 0.25;

constexpr float TORQUE_L = 1;
constexpr float TORQUE_R = 1;

const char* vertex_shader = R"glsl(
    #version 330

    layout(location = 0) in vec3 position;
    uniform vec2 offset;
    void main()
    {
        vec4 totalOffset = vec4(offset.x, offset.y, 0.0, 0.0);
        gl_Position = vec4(position, 1.0) + totalOffset;
    }
)glsl";
const char* fragment_shader = R"glsl(
    #version 330
    out vec4 frag_colour;
    void main() {
      frag_colour = vec4(1.0, 1.0, 1.0, 1.0);
    }
)glsl";

float x = 0, y = 0;

void processInput(GLFWwindow* window, double dt) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
        y += dt * VELOCITY_Y;
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
        y -= dt * VELOCITY_Y;
}

int main(void) {
    // Initialize GLFW
    glewExperimental = true; // Needed for core profile
    if( !glfwInit() )
    {
        fprintf( stderr, "Failed to initialize GLFW\n" );
        return -1;
    }

    glfwWindowHint(GLFW_SAMPLES, 4); // 4x antialiasing
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); // We want OpenGL 3.3
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // To make MacOS happy; should not be needed
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // We don't want the old OpenGL 

    // Open a window and create its OpenGL context
    GLFWwindow* window = glfwCreateWindow( 1024, 768, "Tutorial 01", NULL, NULL);
    if( window == NULL ){
        std::cout << "Failed to create OpenGL window.  Exiting..." << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window); // Initialize GLEW
    glewExperimental=true; // Needed in core profile
    if (glewInit() != GLEW_OK) {
        fprintf(stderr, "Failed to initialize GLEW\n");
        return -1;
    }

    // Ensure we can capture the escape key being pressed below
    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
    
    glfwSetFramebufferSizeCallback(
        window, 
        [](GLFWwindow* window, int width, int height) {
            return glViewport(0, 0, width, height);
        }
    );
   
    // These are the points we're going to render
    float points[] = {
      -0.005f,   0.0f, 0.0f,
       0.005f,   0.0f, 0.0f,
      -0.005f, RADIUS, 0.0f,
       0.005f, RADIUS, 0.0f
    };

    // Here we allocate some memory on the graphics card and move data into the buffer
    GLuint vbo = 0;
    glGenBuffers(1, &vbo);  // Make a new object
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo); // Link the object to our uint vbo
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(points), points, GL_DYNAMIC_DRAW); // Fill the buffer with some data
    
    // Make a vertex array object
    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // This part tells OpenGL that there is useful data in the buffer.  The second function tells OpenGL how to format the information in the buffer.  Data from the AttribArray streams into the vertex_shader.
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);

    // Load the shaders
    GLuint vs = glCreateShader(GL_VERTEX_SHADER); // Makes the shader for OpenGL
    glShaderSource(vs, 1, &vertex_shader, NULL);  // Loads the shader source
    glCompileShader(vs);                          // Compile that shit
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragment_shader, NULL);
    glCompileShader(fs);

    GLuint shader_programme = glCreateProgram();
    glAttachShader(shader_programme, fs);
    glAttachShader(shader_programme, vs);
    glLinkProgram(shader_programme);

    auto offsetLocation = glGetUniformLocation(shader_programme, "offset");
    
    auto time_of_last_frame = std::chrono::system_clock::now();
    auto t_i = time_of_last_frame;
    auto t_im1 = time_of_last_frame;

    do {
        auto t_i = std::chrono::system_clock::now();
        // Handle input
        std::chrono::duration<double> dt = t_i - t_im1;
        processInput(window, dt.count());
        
        // Only draw the last frame at the correct framerate
        std::chrono::duration<double> delta_t = t_i - time_of_last_frame;
        if ( delta_t.count() > SECONDS_BETWEEN_FRAMES ) {
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear( GL_COLOR_BUFFER_BIT );
            glUseProgram(shader_programme);
            glUniform2f(offsetLocation, x, y);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            time_of_last_frame = std::chrono::system_clock::now();
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
        t_im1 = t_i;
    } // Check if the ESC key was pressed or the window was closed
    while( glfwWindowShouldClose(window) == 0 );
    
    glfwTerminate();
    return 0;
}
