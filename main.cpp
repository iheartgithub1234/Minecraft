#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <vector>
#include <iomanip>
#include <sstream>
#include <array>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


// Window dimensions
const int WIDTH = 800, HEIGHT = 600;


// World dimensions
const int CHUNK_SIZE = 16;  // 8x8 chunks for better performance
const int WORLD_HEIGHT = 8;


// Colors
glm::vec3 skyColor = glm::vec3(0.5f, 0.8f, 1.0f);


// Camera
glm::vec3 cameraPos = glm::vec3(4.0f, 5.0f, 4.0f);
glm::vec3 cameraFront = glm::vec3(-0.5f, -0.5f, -0.5f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);


// Mouse
float yaw = -135.0f;
float pitch = -30.0f;
float lastX = WIDTH / 2.0f;
float lastY = HEIGHT / 2.0f;
bool firstMouse = true;
bool cursorVisible = false;


// Timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;
double lastTime = glfwGetTime();
int frameCount = 0;
double fps = 0.0;


// Block system
enum BlockType { AIR, DIRT, COBBLESTONE, SAND, WOOD, GLASS };
BlockType currentBlock = DIRT;
std::vector<std::vector<std::vector<BlockType>>> blocks(
   CHUNK_SIZE,
   std::vector<std::vector<BlockType>>(
       WORLD_HEIGHT,
       std::vector<BlockType>(CHUNK_SIZE, AIR)
   )
);


// Texture
unsigned int textureID;


// Crosshair
unsigned int crosshairVAO, crosshairVBO;
unsigned int crosshairShader;


// Wireframe mode
bool wireframeMode = false;


const char* getBlockName(BlockType type) {
   switch(type) {
       case DIRT: return "Dirt";
       case COBBLESTONE: return "Cobblestone";
       case SAND: return "Sand";
       case WOOD: return "Wood";
       case GLASS: return "Glass";
       default: return "Air";
   }
}


struct FaceUVs {
   float u0, u1, v0, v1;
};


FaceUVs getFaceUVs(BlockType type, const char* face) {
   float texSize = 16.0f / 128.0f;
   switch(type) {
       case DIRT:
           if (strcmp(face, "top") == 0) return {2*texSize, 3*texSize, texSize, 0.0f};
           if (strcmp(face, "bottom") == 0) return {texSize, 2*texSize, texSize, 0.0f};
           return {0.0f, texSize, texSize, 0.0f};
       case COBBLESTONE:
           return {0.0f, texSize, texSize, 2*texSize};
       case SAND:
           return {0.0f, texSize, 2*texSize, 3*texSize};
       case WOOD:
           return {0.0f, texSize, 3*texSize, 4*texSize};
       case GLASS:
           return {0.0f, texSize, 4*texSize, 5*texSize};
       default:
           return {0.0f, texSize, 0.0f, texSize};
   }
}


struct RaycastResult {
   bool hit;
   glm::ivec3 blockPos;
   glm::ivec3 normal;
};


RaycastResult rayCast(glm::vec3 start, glm::vec3 direction, float maxDist) {
   RaycastResult result;
   result.hit = false;


   glm::ivec3 mapPos(glm::floor(start));
   glm::vec3 deltaDist(
       abs(1.0f / direction.x),
       abs(1.0f / direction.y),
       abs(1.0f / direction.z)
   );
  
   glm::ivec3 step(
       direction.x > 0 ? 1 : -1,
       direction.y > 0 ? 1 : -1,
       direction.z > 0 ? 1 : -1
   );
  
   glm::vec3 sideDist(
       direction.x > 0 ? (mapPos.x + 1 - start.x) * deltaDist.x : (start.x - mapPos.x) * deltaDist.x,
       direction.y > 0 ? (mapPos.y + 1 - start.y) * deltaDist.y : (start.y - mapPos.y) * deltaDist.y,
       direction.z > 0 ? (mapPos.z + 1 - start.z) * deltaDist.z : (start.z - mapPos.z) * deltaDist.z
   );


   float maxSide = 0.0f;
   int side = 0;
   float traveled = 0.0f;


   while (traveled < maxDist) {
       if (sideDist.x < sideDist.y) {
           if (sideDist.x < sideDist.z) {
               sideDist.x += deltaDist.x;
               mapPos.x += step.x;
               side = 0;
               maxSide = sideDist.x - deltaDist.x;
           } else {
               sideDist.z += deltaDist.z;
               mapPos.z += step.z;
               side = 2;
               maxSide = sideDist.z - deltaDist.z;
           }
       } else {
           if (sideDist.y < sideDist.z) {
               sideDist.y += deltaDist.y;
               mapPos.y += step.y;
               side = 1;
               maxSide = sideDist.y - deltaDist.y;
           } else {
               sideDist.z += deltaDist.z;
               mapPos.z += step.z;
               side = 2;
               maxSide = sideDist.z - deltaDist.z;
           }
       }


       traveled = maxSide;
      
       if (mapPos.x < 0 || mapPos.x >= CHUNK_SIZE ||
           mapPos.y < 0 || mapPos.y >= WORLD_HEIGHT ||
           mapPos.z < 0 || mapPos.z >= CHUNK_SIZE) break;


       if (blocks[mapPos.x][mapPos.y][mapPos.z] != AIR) {
           result.hit = true;
           result.blockPos = mapPos;
          
           switch(side) {
               case 0: result.normal = glm::ivec3(-step.x, 0, 0); break;
               case 1: result.normal = glm::ivec3(0, -step.y, 0); break;
               case 2: result.normal = glm::ivec3(0, 0, -step.z); break;
           }
           return result;
       }
   }
   return result;
}


void handleBlockInteraction(GLFWwindow* window, int button, int action) {
   if (!cursorVisible && action == GLFW_PRESS) {
       RaycastResult rc = rayCast(cameraPos, cameraFront, 8.0f);
       if (rc.hit) {
           if (button == GLFW_MOUSE_BUTTON_LEFT) {
               blocks[rc.blockPos.x][rc.blockPos.y][rc.blockPos.z] = AIR;
           } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
               glm::ivec3 newPos = rc.blockPos + rc.normal;
               if (newPos.x >= 0 && newPos.x < CHUNK_SIZE &&
                   newPos.y >= 0 && newPos.y < WORLD_HEIGHT &&
                   newPos.z >= 0 && newPos.z < CHUNK_SIZE) {
                   blocks[newPos.x][newPos.y][newPos.z] = currentBlock;
               }
           }
       }
   }
}


void toggleCursor(GLFWwindow* window) {
   cursorVisible = !cursorVisible;
   if (cursorVisible) {
       glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
       firstMouse = true;
   } else {
       glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
   }
}


void printStats() {
   double currentTime = glfwGetTime();
   frameCount++;
  
   if (currentTime - lastTime >= 0.25) {
       fps = frameCount / (currentTime - lastTime);
       frameCount = 0;
       lastTime = currentTime;


       const char* fpsColor;
       if (fps < 30) fpsColor = "\033[31m";
       else if (fps < 60) fpsColor = "\033[33m";
       else fpsColor = "\033[32m";


       std::ostringstream coordStream;
       coordStream << std::fixed << std::setprecision(2);
       coordStream << "x: " << cameraPos.x << ", y: " << cameraPos.y << ", z: " << cameraPos.z;


       std::cout << "\r\033[K";
       std::cout << "\033[37mFPS: " << fpsColor << static_cast<int>(fps) << "\033[0m";
       std::cout << " | \033[94m" << coordStream.str() << "\033[0m";
       std::cout << " | \033[95mWireframe: " << (wireframeMode ? "ON" : "OFF") << "\033[0m";
       std::cout << " | \033[96mBlock: " << getBlockName(currentBlock) << "\033[0m" << std::flush;
   }
}


void processInput(GLFWwindow* window) {
   if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
       toggleCursor(window);
       while (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
           glfwPollEvents();
       }
   }


   if (glfwGetKey(window, GLFW_KEY_BACKSPACE) == GLFW_PRESS) {
       glfwSetWindowShouldClose(window, true);
   }


   static bool enterKeyPressed = false;
   if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS) {
       if (!enterKeyPressed) {
           wireframeMode = !wireframeMode;
           enterKeyPressed = true;
       }
   } else {
       enterKeyPressed = false;
   }


   if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) currentBlock = DIRT;
   if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) currentBlock = COBBLESTONE;
   if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) currentBlock = SAND;
   if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS) currentBlock = WOOD;
   if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS) currentBlock = GLASS;


   if (!cursorVisible) {
       float cameraSpeed = 5.0f * deltaTime;
       if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
           cameraPos += cameraSpeed * cameraFront;
       if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
           cameraPos -= cameraSpeed * cameraFront;
       if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
           cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
       if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
           cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
       if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
           cameraPos += cameraSpeed * cameraUp;
       if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
           cameraPos -= cameraSpeed * cameraUp;
   }
}


void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
   handleBlockInteraction(window, button, action);
}


void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
   if (cursorVisible) return;


   if (firstMouse) {
       lastX = xpos;
       lastY = ypos;
       firstMouse = false;
   }


   float xoffset = xpos - lastX;
   float yoffset = lastY - ypos;
   lastX = xpos;
   lastY = ypos;


   float sensitivity = 0.1f;
   xoffset *= sensitivity;
   yoffset *= sensitivity;


   yaw += xoffset;
   pitch += yoffset;


   if (pitch > 89.0f)
       pitch = 89.0f;
   if (pitch < -89.0f)
       pitch = -89.0f;


   glm::vec3 front;
   front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
   front.y = sin(glm::radians(pitch));
   front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
   cameraFront = glm::normalize(front);
}


void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
   glViewport(0, 0, width, height);
}


unsigned int loadTexture(const char* path) {
   unsigned int texture;
   glGenTextures(1, &texture);
   glBindTexture(GL_TEXTURE_2D, texture);
  
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  
   int width, height, nrChannels;
   unsigned char* data = stbi_load(path, &width, &height, &nrChannels, STBI_rgb_alpha);
   if (data) {
       glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
       glGenerateMipmap(GL_TEXTURE_2D);
   } else {
       std::cout << "Failed to load texture" << std::endl;
   }
   stbi_image_free(data);
  
   return texture;
}


void drawCube(float x, float y, float z, BlockType type) {
   FaceUVs front = getFaceUVs(type, "front");
   FaceUVs back = getFaceUVs(type, "back");
   FaceUVs left = getFaceUVs(type, "left");
   FaceUVs right = getFaceUVs(type, "right");
   FaceUVs top = getFaceUVs(type, "top");
   FaceUVs bottom = getFaceUVs(type, "bottom");


   float x0 = x;
   float x1 = x + 1.0f;
   float y0 = y;
   float y1 = y + 1.0f;
   float z0 = z;
   float z1 = z + 1.0f;


   float vertices[] = {
       // Back face
       x0, y0, z0,   back.u0, back.v0,
       x1, y0, z0,   back.u1, back.v0,
       x1, y1, z0,   back.u1, back.v1,
       x1, y1, z0,   back.u1, back.v1,
       x0, y1, z0,   back.u0, back.v1,
       x0, y0, z0,   back.u0, back.v0,


       // Front face
       x0, y0, z1,   front.u0, front.v0,
       x1, y0, z1,   front.u1, front.v0,
       x1, y1, z1,   front.u1, front.v1,
       x1, y1, z1,   front.u1, front.v1,
       x0, y1, z1,   front.u0, front.v1,
       x0, y0, z1,   front.u0, front.v0,


       // Left face
       x0, y1, z1,   left.u0, left.v1,
       x0, y1, z0,   left.u1, left.v1,
       x0, y0, z0,   left.u1, left.v0,
       x0, y0, z0,   left.u1, left.v0,
       x0, y0, z1,   left.u0, left.v0,
       x0, y1, z1,   left.u0, left.v1,


       // Right face
       x1, y1, z1,   right.u1, right.v1,
       x1, y1, z0,   right.u0, right.v1,
       x1, y0, z0,   right.u0, right.v0,
       x1, y0, z0,   right.u0, right.v0,
       x1, y0, z1,   right.u1, right.v0,
       x1, y1, z1,   right.u1, right.v1,


       // Bottom face
       x0, y0, z0,   bottom.u0, bottom.v1,
       x1, y0, z0,   bottom.u1, bottom.v1,
       x1, y0, z1,   bottom.u1, bottom.v0,
       x1, y0, z1,   bottom.u1, bottom.v0,
       x0, y0, z1,   bottom.u0, bottom.v0,
       x0, y0, z0,   bottom.u0, bottom.v1,


       // Top face
       x0, y1, z0,   top.u0, top.v1,
       x1, y1, z0,   top.u1, top.v1,
       x1, y1, z1,   top.u1, top.v0,
       x1, y1, z1,   top.u1, top.v0,
       x0, y1, z1,   top.u0, top.v0,
       x0, y1, z0,   top.u0, top.v1
   };


   unsigned int VBO, VAO;
   glGenVertexArrays(1, &VAO);
   glGenBuffers(1, &VBO);


   glBindVertexArray(VAO);
   glBindBuffer(GL_ARRAY_BUFFER, VBO);
   glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);


   glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
   glEnableVertexAttribArray(0);
   glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
   glEnableVertexAttribArray(1);


   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, textureID);


   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glBindVertexArray(VAO);
  
   if (type == GLASS) {
       glEnable(GL_BLEND);
       glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   }
  
   if (wireframeMode) {
       glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
       glDisable(GL_TEXTURE_2D);
       glDrawArrays(GL_TRIANGLES, 0, 36);
       glEnable(GL_TEXTURE_2D);
       glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
   } else {
       glDrawArrays(GL_TRIANGLES, 0, 36);
   }
  
   if (type == GLASS) {
       glDisable(GL_BLEND);
   }
  
   glBindVertexArray(0);
  
   glDeleteVertexArrays(1, &VAO);
   glDeleteBuffers(1, &VBO);
}


void initCrosshair() {
   float size = 0.02f;
   float thickness = 0.005f;
   float aspectRatio = (float)WIDTH / (float)HEIGHT;
   float verticalSize = size * aspectRatio;
  
   std::array<float, 32> vertices = {
       -size, -thickness/2, size, -thickness/2,
       size, thickness/2, -size, thickness/2,
       -thickness/2, -verticalSize, thickness/2, -verticalSize,
       thickness/2, verticalSize, -thickness/2, verticalSize
   };
  
   glGenVertexArrays(1, &crosshairVAO);
   glGenBuffers(1, &crosshairVBO);
  
   glBindVertexArray(crosshairVAO);
   glBindBuffer(GL_ARRAY_BUFFER, crosshairVBO);
   glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices.data(), GL_STATIC_DRAW);
  
   glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
   glEnableVertexAttribArray(0);
  
   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glBindVertexArray(0);
  
   const char* crosshairVertexShaderSource = "#version 330 core\n"
       "layout (location = 0) in vec2 aPos;\n"
       "void main() {\n"
       "   gl_Position = vec4(aPos, 0.0, 1.0);\n"
       "}\0";


   const char* crosshairFragmentShaderSource = "#version 330 core\n"
       "out vec4 FragColor;\n"
       "void main() {\n"
       "   FragColor = vec4(1.0, 1.0, 1.0, 0.8);\n"
       "}\0";


   unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
   glShaderSource(vertexShader, 1, &crosshairVertexShaderSource, NULL);
   glCompileShader(vertexShader);
  
   unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
   glShaderSource(fragmentShader, 1, &crosshairFragmentShaderSource, NULL);
   glCompileShader(fragmentShader);
  
   crosshairShader = glCreateProgram();
   glAttachShader(crosshairShader, vertexShader);
   glAttachShader(crosshairShader, fragmentShader);
   glLinkProgram(crosshairShader);
  
   glDeleteShader(vertexShader);
   glDeleteShader(fragmentShader);
}


void renderCrosshair() {
   glDisable(GL_DEPTH_TEST);
  
   glUseProgram(crosshairShader);
   glBindVertexArray(crosshairVAO);
  
   glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
   glDrawArrays(GL_TRIANGLE_FAN, 4, 4);
  
   glBindVertexArray(0);
   glEnable(GL_DEPTH_TEST);
}


int main() {
   if (!glfwInit()) {
       std::cerr << "Failed to initialize GLFW" << std::endl;
       return -1;
   }


   glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
   glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
   glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);


   GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Minecraft-like Platform", NULL, NULL);
   if (!window) {
       std::cerr << "Failed to create GLFW window" << std::endl;
       glfwTerminate();
       return -1;
   }


   glfwMakeContextCurrent(window);
   glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
   glfwSetCursorPosCallback(window, mouse_callback);
   glfwSetMouseButtonCallback(window, mouse_button_callback);
   glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);


   if (glewInit() != GLEW_OK) {
       std::cerr << "Failed to initialize GLEW" << std::endl;
       return -1;
   }


   // Initialize world with 8x8 platform and small hill
   for (int x = 0; x < CHUNK_SIZE; x++) {
       for (int z = 0; z < CHUNK_SIZE; z++) {
           blocks[x][0][z] = DIRT;
       }
   }


   glEnable(GL_DEPTH_TEST);
   textureID = loadTexture("assets/atlas.png");
   initCrosshair();


   // Main shader program
   const char* vertexShaderSource = "#version 330 core\n"
       "layout (location = 0) in vec3 aPos;\n"
       "layout (location = 1) in vec2 aTexCoord;\n"
       "out vec2 TexCoord;\n"
       "uniform mat4 model;\n"
       "uniform mat4 view;\n"
       "uniform mat4 projection;\n"
       "void main() {\n"
       "   gl_Position = projection * view * model * vec4(aPos, 1.0);\n"
       "   TexCoord = aTexCoord;\n"
       "}\0";


   const char* fragmentShaderSource = "#version 330 core\n"
       "in vec2 TexCoord;\n"
       "out vec4 FragColor;\n"
       "uniform sampler2D ourTexture;\n"
       "void main() {\n"
       "   vec4 texColor = texture(ourTexture, TexCoord);\n"
       "   if(texColor.a < 0.1) discard;\n"
       "   FragColor = texColor;\n"
       "}\0";


   unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
   glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
   glCompileShader(vertexShader);


   unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
   glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
   glCompileShader(fragmentShader);


   unsigned int shaderProgram = glCreateProgram();
   glAttachShader(shaderProgram, vertexShader);
   glAttachShader(shaderProgram, fragmentShader);
   glLinkProgram(shaderProgram);


   glDeleteShader(vertexShader);
   glDeleteShader(fragmentShader);


   while (!glfwWindowShouldClose(window)) {
       float currentFrame = glfwGetTime();
       deltaTime = currentFrame - lastFrame;
       lastFrame = currentFrame;


       processInput(window);
       printStats();


       glClearColor(skyColor.r, skyColor.g, skyColor.b, 1.0f);
       glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


       glUseProgram(shaderProgram);
       glUniform1i(glGetUniformLocation(shaderProgram, "ourTexture"), 0);


       glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
       glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 100.0f);


       glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, &view[0][0]);
       glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, &projection[0][0]);


       // Draw all opaque blocks first
       for (int x = 0; x < CHUNK_SIZE; x++) {
           for (int y = 0; y < WORLD_HEIGHT; y++) {
               for (int z = 0; z < CHUNK_SIZE; z++) {
                   if (blocks[x][y][z] != AIR && blocks[x][y][z] != GLASS) {
                       glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));
                       glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, &model[0][0]);
                       drawCube(0, 0, 0, blocks[x][y][z]);
                   }
               }
           }
       }


       // Then draw transparent blocks (glass)
       for (int x = 0; x < CHUNK_SIZE; x++) {
           for (int y = 0; y < WORLD_HEIGHT; y++) {
               for (int z = 0; z < CHUNK_SIZE; z++) {
                   if (blocks[x][y][z] == GLASS) {
                       glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));
                       glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, &model[0][0]);
                       drawCube(0, 0, 0, blocks[x][y][z]);
                   }
               }
           }
       }


       renderCrosshair();
      
       glfwSwapBuffers(window);
       glfwPollEvents();
   }


   std::cout << "\n";
   glfwTerminate();
   return 0;
}