#include <tiny_obj_loader.h>
#include <GL/glew.h>

#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <cmath> // For sin and cos functions

// GLM for matrix operations
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Allows text to be printed from text file
#include <fstream>
#include <sstream>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <map>

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// Vertex Shader Source for the model
const char* vertexShaderSource = R"glsl(
    #version 330 core
    layout(location = 0) in vec3 aPos;
    layout(location = 1) in vec3 aNormal;

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;

    out vec3 FragPos;  
    out vec3 Normal;  

    void main() {
        FragPos = vec3(model * vec4(aPos, 1.0));  
        Normal = mat3(transpose(inverse(model))) * aNormal;  

        gl_Position = projection * view * vec4(FragPos, 1.0);
    }
)glsl";

// Fragment Shader Source for the model
const char* fragmentShaderSource = R"glsl(
    #version 330 core
    out vec4 FragColor;

    in vec3 FragPos;  
    in vec3 Normal;  

    // Light and material properties
    uniform vec3 lightPos; 
    uniform vec3 viewPos; 
    uniform vec3 lightColor;
    uniform vec3 objectColor;

    void main() {
        // Ambient
        float ambientStrength = 0.1;
        vec3 ambient = ambientStrength * lightColor;
      	
        // Diffuse 
        vec3 norm = normalize(Normal);
        vec3 lightDir = normalize(lightPos - FragPos);  
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = diff * lightColor;
        
        // Specular
        float specularStrength = 0.5;
        vec3 viewDir = normalize(viewPos - FragPos);  
        vec3 reflectDir = reflect(-lightDir, norm);  
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
        vec3 specular = specularStrength * spec * lightColor;  
            
        vec3 result = (ambient + diffuse + specular) * objectColor;
        FragColor = vec4(result, 1.0);
    }
)glsl";

// Vertex Shader Source for the axes
const char* axesVertexShaderSource = R"glsl(
    #version 330 core
    layout(location = 0) in vec3 aPos;
    layout(location = 1) in vec3 aColor;

    uniform mat4 view;
    uniform mat4 projection;

    out vec3 vertexColor;

    void main() {
        vertexColor = aColor;
        gl_Position = projection * view * vec4(aPos, 1.0);
    }
)glsl";

// Fragment Shader Source for the axes
const char* axesFragmentShaderSource = R"glsl(
    #version 330 core
    in vec3 vertexColor;
    out vec4 FragColor;

    void main() {
        FragColor = vec4(vertexColor, 1.0);
    }
)glsl";

// Post-processing shader
const char* postProcessingVertexShaderSource = R"glsl(
    #version 330 core
    layout(location = 0) in vec2 aPos;
    layout(location = 1) in vec2 aTexCoords;

    out vec2 TexCoords;

    void main() {
        TexCoords = aTexCoords;
        gl_Position = vec4(aPos, 0.0, 1.0);
    }
)glsl";

const char* postProcessingFragmentShaderSource = R"glsl(
    #version 330 core
    out vec4 FragColor;

    in vec2 TexCoords;

    uniform sampler2D screenTexture;

    void main() {
        FragColor = texture(screenTexture, TexCoords);
    }
)glsl";

// Global variables for rotation and movement
glm::vec3 modelPosition = glm::vec3(0.0f, 0.0f, 0.0f);
float rotationY = 0.0f; // Yaw rotation
const float rotationSpeed = 0.01f;
const float movementSpeed = 0.05f;

// Function prototypes
void processInput(GLFWwindow* window);
void checkGLError(const std::string& errorMessage);

// Define the Character structure for text display
struct Character 
{
    GLuint TextureID;   // ID of the glyph
    glm::ivec2 Size;    // Size 
    glm::ivec2 Bearing; // Offset from baseline 
    GLuint Advance;     // Offset to advance to next char (glyph)
};

enum GameState 
{
    Start_Screen,
    Lore_Screen,
    Game_Screen,
    End_screen
};

GameState gameState = Lore_Screen;

void handleInput(GLFWwindow* window) 
{
    if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS) 
    {
        gameState = Game_Screen;
    }
}

int main() 
{
    // Initialize GLFW
    if (!glfwInit()) 
    {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // Set up OpenGL version and profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create window
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "3D Model Loader with Axes Visualization", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    // Set current context
    glfwMakeContextCurrent(window);

    // Initialize GLEW
    glewExperimental = GL_TRUE; // Needed for core profile
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return -1;
    }

    checkGLError("GLEW initialization error");
    // Set up rendering
    //---------------------------------------------------------------------------------------------------------------------------------------------------------------
    // Enable depth testing
    glEnable(GL_DEPTH_TEST);

    // Build and compile shaders for the model
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    checkGLError("Vertex shader compilation error");

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    checkGLError("Fragment shader compilation error");

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    checkGLError("Shader program linking error");

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Build and compile shaders for the axes
    unsigned int axesVertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(axesVertexShader, 1, &axesVertexShaderSource, NULL);
    glCompileShader(axesVertexShader);
    checkGLError("Axes vertex shader compilation error");

    unsigned int axesFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(axesFragmentShader, 1, &axesFragmentShaderSource, NULL);
    glCompileShader(axesFragmentShader);
    checkGLError("Axes fragment shader compilation error");

    unsigned int axesShaderProgram = glCreateProgram();
    glAttachShader(axesShaderProgram, axesVertexShader);
    glAttachShader(axesShaderProgram, axesFragmentShader);
    glLinkProgram(axesShaderProgram);
    checkGLError("Axes shader program linking error");

    glDeleteShader(axesVertexShader);
    glDeleteShader(axesFragmentShader);

    // Load .obj file
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    std::string inputfile = "./BlenderObjects/Spaceship2.obj";
    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, inputfile.c_str());

    if (!warn.empty()) {
        std::cout << "WARN: " << warn << std::endl;
    }

    if (!err.empty()) {
        std::cerr << "ERR: " << err << std::endl;
    }

    if (!ret) {
        std::cerr << "Failed to load .obj file!" << std::endl;
        return -1;
    }

    // Prepare vertex data for the model
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    for (size_t s = 0; s < shapes.size(); s++) {
        size_t index_offset = 0;
        for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
            int fv = shapes[s].mesh.num_face_vertices[f];

            // Process per-face
            for (size_t v = 0; v < fv; v++) {
                // Access vertex data
                tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
                tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
                tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
                tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];

                tinyobj::real_t nx = 0;
                tinyobj::real_t ny = 0;
                tinyobj::real_t nz = 0;
                if (idx.normal_index >= 0) {
                    nx = attrib.normals[3 * idx.normal_index + 0];
                    ny = attrib.normals[3 * idx.normal_index + 1];
                    nz = attrib.normals[3 * idx.normal_index + 2];
                }

                // Append vertex data
                vertices.push_back(vx);
                vertices.push_back(vy);
                vertices.push_back(vz);
                vertices.push_back(nx);
                vertices.push_back(ny);
                vertices.push_back(nz);

                indices.push_back(indices.size());
            }
            index_offset += fv;
        }
    }

    // Setup buffers and arrays for the model
    unsigned int VBO, VAO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    // Bind buffers for the model
    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // Vertex positions
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Vertex normals
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    checkGLError("Vertex attribute setup error");

    // Prepare vertex data for the axes
    float axesVertices[] = {
        // Positions          // Colors
        // X-axis (Red)
        0.0f, 0.0f, 0.0f,     1.0f, 0.0f, 0.0f, // Origin
        10.0f, 0.0f, 0.0f,    1.0f, 0.0f, 0.0f, // Positive X direction

        // Y-axis (Green)
        0.0f, 0.0f, 0.0f,     0.0f, 1.0f, 0.0f, // Origin
        0.0f, 10.0f, 0.0f,    0.0f, 1.0f, 0.0f, // Positive Y direction

        // Z-axis (Blue)
        0.0f, 0.0f, 0.0f,     0.0f, 0.0f, 1.0f, // Origin
        0.0f, 0.0f, 10.0f,    0.0f, 0.0f, 1.0f  // Positive Z direction
    };

    // Generate buffers and arrays for the axes
    unsigned int axesVAO, axesVBO;
    glGenVertexArrays(1, &axesVAO);
    glGenBuffers(1, &axesVBO);

    // Bind and set up axes VAO and VBO
    glBindVertexArray(axesVAO);

    glBindBuffer(GL_ARRAY_BUFFER, axesVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(axesVertices), axesVertices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Color attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    checkGLError("Axes attribute setup error");

    // Get uniform locations for the model shader
    unsigned int modelLoc = glGetUniformLocation(shaderProgram, "model");
    unsigned int viewLoc  = glGetUniformLocation(shaderProgram, "view");
    unsigned int projLoc  = glGetUniformLocation(shaderProgram, "projection");
    //---------------------------------------------------------------------------------------------------------------------------------------------------------------

    //---------------------------------------------------- Post-processing setup ------------------------------------------------------------------------------------
    
    // Framebuffer configuration
    unsigned int framebuffer;
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    // Create a color attachment texture
    unsigned int textureColorbuffer;
    glGenTextures(1, &textureColorbuffer);
    glBindTexture(GL_TEXTURE_2D, textureColorbuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureColorbuffer, 0);

    // Create a renderbuffer object for depth and stencil attachment (we won't be sampling these)
    unsigned int rbo;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, SCR_WIDTH, SCR_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

    // Check if framebuffer is complete
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Screen quad VAO
    float quadVertices[] = {
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };

    unsigned int quadVAO, quadVBO;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    unsigned int postProcessingVertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(postProcessingVertexShader, 1, &postProcessingVertexShaderSource, NULL);
    glCompileShader(postProcessingVertexShader);
    checkGLError("Post-processing vertex shader compilation error");

    unsigned int postProcessingFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(postProcessingFragmentShader, 1, &postProcessingFragmentShaderSource, NULL);
    glCompileShader(postProcessingFragmentShader);
    checkGLError("Post-processing fragment shader compilation error");

    unsigned int postProcessingShaderProgram = glCreateProgram();
    glAttachShader(postProcessingShaderProgram, postProcessingVertexShader);
    glAttachShader(postProcessingShaderProgram, postProcessingFragmentShader);
    glLinkProgram(postProcessingShaderProgram);
    checkGLError("Post-processing shader program linking error");

    glDeleteShader(postProcessingVertexShader);
    glDeleteShader(postProcessingFragmentShader);

    //---------------------------------------------------- Freetype setup ------------------------------------------------------------------------------------
    // Initialize FreeType
            FT_Library ft;
            if (FT_Init_FreeType(&ft)) {
                std::cerr << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
            }

            // Load font as face
            FT_Face face;
            if (FT_New_Face(ft, "c:/WINDOWS/Fonts/VGAFIX.FON", 0, &face)) {
                std::cerr << "ERROR::FREETYPE: Failed to load font" << std::endl;
            }

            // Set size to load glyphs as
            FT_Set_Pixel_Sizes(face, 0, 48);

            // Disable byte-alignment restriction
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

            // Load first 128 characters of ASCII set
            std::map<GLchar, Character> Characters;
            for (GLubyte c = 0; c < 128; c++) {
                // Load character glyph 
                if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
                    std::cerr << "ERROR::FREETYPE: Failed to load Glyph" << std::endl;
                    continue;
                }
                // Generate texture
                GLuint texture;
                glGenTextures(1, &texture);
                glBindTexture(GL_TEXTURE_2D, texture);
                glTexImage2D(
                    GL_TEXTURE_2D,
                    0,
                    GL_RED,
                    face->glyph->bitmap.width,
                    face->glyph->bitmap.rows,
                    0,
                    GL_RED,
                    GL_UNSIGNED_BYTE,
                    face->glyph->bitmap.buffer
                );
                // Set texture options
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                // Now store character for later use
                Character character = {
                    texture,
                    glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
                    glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
                    face->glyph->advance.x
                };
                Characters.insert(std::pair<GLchar, Character>(c, character));
            }
            glBindTexture(GL_TEXTURE_2D, 0);

            // Destroy FreeType once we're finished
            FT_Done_Face(face);
            FT_Done_FreeType(ft);
    //---------------------------------------------------------------------------------------------------------------------------------------------------------------
    // Main loop
    while (!glfwWindowShouldClose(window)) 
    {
        // Input
        processInput(window);
        //---------------------------------------------------------------------------------------------------------------------------------------------------------------
        // If statements dictate the current state of the game
        if(gameState == Start_Screen)
        {
            //TODO: Add code to render the start screen
            glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            //TODO: Add code to render "Start Game" text or button
        }
        //---------------------------------------------------------------------------------------------------------------------------------------------------------------
        else if (gameState == Lore_Screen) 
        {
            // Clear the screen with black color
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            // Load the lore text from file
            static std::string loreText;
            static bool loreLoaded = false;
            if (!loreLoaded) 
            {
                std::ifstream loreFile("src/Lore_text_file.txt");
                if (!loreFile.is_open()) 
                {
                    std::cerr << "Failed to open lore text file!" << std::endl;
                    glfwSetWindowShouldClose(window, true);
                    break;
                }

                std::stringstream loreStream;
                loreStream << loreFile.rdbuf();
                loreText = loreStream.str();
                loreFile.close();
                loreLoaded = true;
            }

            // Print the lore text slowly
            static size_t charIndex = 0;
            static float timeAccumulator = 0.0f;
            const float charDisplayInterval = 0.05f; // Time interval between characters

            timeAccumulator += glfwGetTime();
            glfwSetTime(0.0);

            if (timeAccumulator >= charDisplayInterval && charIndex < loreText.size()) 
            {
                charIndex++;
                timeAccumulator = 0.0f;
            }

            std::string displayedText = loreText.substr(0, charIndex);

            // Render the displayed text
            // Activate corresponding render state	
            glUseProgram(postProcessingShaderProgram);
            glUniform3f(glGetUniformLocation(postProcessingShaderProgram, "textColor"), 1.0f, 1.0f, 1.0f);
            glActiveTexture(GL_TEXTURE0);
            glBindVertexArray(quadVAO);

            // Iterate through all characters
            std::string::const_iterator c;
            float x = 25.0f; // Starting position
            float y = 300.0f; // Starting position
            float scale = 1.0f; // Scale of the text

            for (c = displayedText.begin(); c != displayedText.end(); c++) 
            {
                Character ch = Characters[*c];

                float xpos = x + ch.Bearing.x * scale;
                float ypos = y - (ch.Size.y - ch.Bearing.y) * scale;

                float w = ch.Size.x * scale;
                float h = ch.Size.y * scale;
                // Update VBO for each character
                float vertices[6][4] = {
                    { xpos,     ypos + h,   0.0f, 0.0f },            
                    { xpos,     ypos,       0.0f, 1.0f },
                    { xpos + w, ypos,       1.0f, 1.0f },

                    { xpos,     ypos + h,   0.0f, 0.0f },
                    { xpos + w, ypos,       1.0f, 1.0f },
                    { xpos + w, ypos + h,   1.0f, 0.0f }           
                };
                // Render glyph texture over quad
                glBindTexture(GL_TEXTURE_2D, ch.TextureID);
                // Update content of VBO memory
                glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices); 
                glBindBuffer(GL_ARRAY_BUFFER, 0);
                // Render quad
                glDrawArrays(GL_TRIANGLES, 0, 6);
                // Advance cursors for next glyph (advance is number of 1/64 pixels)
                x += (ch.Advance >> 6) * scale; // Bitshift by 6 to get value in pixels (2^6 = 64)
            }
            glBindVertexArray(0);
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        //---------------------------------------------------------------------------------------------------------------------------------------------------------------
        else if(gameState == Game_Screen)
        {
            // Render
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // // Transformations for the model
            glm::mat4 model = glm::mat4(1.0f);

            // Rotate to make Z-axis point up
            model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)); // Rotate around X-axis

            // Apply translation based on modelPosition
            model = glm::translate(model, modelPosition);

            // Apply rotation around new Y-axis (previously Z-axis)
            model = glm::rotate(model, rotationY, glm::vec3(0.0f, 0.0f, 1.0f));

            // Camera settings
            //glm::vec3 cameraOffset = glm::vec3(30.0f, 0.0f, 15.0f); // Adjust offsets as needed
            glm::vec3 cameraOffset = glm::vec3(30.0f, 30.0f, 30.0f); // checking if the obj is moving linearly in the axes
            glm::vec3 cameraPos = cameraOffset; // modelPosition + cameraOffset;
            glm::vec3 target = modelPosition;
            glm::vec3 up = glm::vec3(.0f, 0.0f, 1.0f);
            glm::mat4 view = glm::lookAt(cameraPos, target, up);

            // Projection
            glm::mat4 projection = glm::perspective(glm::radians(45.0f),
                    (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);

            // Render the axes
            glUseProgram(axesShaderProgram);
            glUniformMatrix4fv(glGetUniformLocation(axesShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(axesShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
            glBindVertexArray(axesVAO);

            // // Optionally set line width
            glLineWidth(2.0f);

            // Draw the axes
            glDrawArrays(GL_LINES, 0, 6);
            // Render the model
            glUseProgram(shaderProgram);

            // Set uniforms for the model shader
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
            glUniformMatrix4fv(viewLoc,  1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(projLoc,  1, GL_FALSE, glm::value_ptr(projection));

            // Update viewPos uniform
            glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, glm::value_ptr(cameraPos));

            // Light and material properties
            glUniform3f(glGetUniformLocation(shaderProgram, "lightPos"), 50.0f, 50.0f, 50.0f);
            glUniform3f(glGetUniformLocation(shaderProgram, "lightColor"), 1.0f, 1.0f, 1.0f);
            glUniform3f(glGetUniformLocation(shaderProgram, "objectColor"), 0.6f, 0.6f, 0.6f);

            // Render the model
            glBindVertexArray(VAO);
            glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
        }
        else if(gameState == End_screen)
        {

        }
        //---------------------------------------------------------------------------------------------------------------------------------------------------------------
        // Swap buffers and poll IO events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Clean up resources
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);

    glDeleteVertexArrays(1, &axesVAO);
    glDeleteBuffers(1, &axesVBO);

    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow* window) {
    // Close window
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Calculate forward vector based on current rotation
    glm::vec3 forward = glm::vec3(-cos(rotationY), -sin(rotationY), 0.0f);
    glm::vec3 right = glm::vec3(-forward.y, forward.x, 0.0f); // Right vector is perpendicular to forward

    // Forward/backward movement - moves along the x axis
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
        //modelPosition += forward * movementSpeed;
         modelPosition.x -= movementSpeed;
    }
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
        //modelPosition -= forward * movementSpeed;
        modelPosition.x += movementSpeed;
    }

    // Left/right strafing movement - moves along the z axis
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
        //modelPosition -= right * movementSpeed;
        modelPosition.z += movementSpeed;
    }
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
        //modelPosition += right * movementSpeed;
        modelPosition.z -= movementSpeed;
    }
}

// Function to check for OpenGL errors
void checkGLError(const std::string& errorMessage) {
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        std::cerr << errorMessage << ": OpenGL error: " << err << std::endl;
    }
}
