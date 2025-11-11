#  Compgrafmidka
### Character Control with WASD and Biped Skeletal Animation

---

##  Course
**Computer Graphics Fundamentals**

---

##  Project Overview
This project is a prototype of a **3D character controller** featuring **WASD movement**, **biped skeletal animation**, and **basic physics**.  
The goal is to demonstrate fundamental concepts of **interactive graphics programming** and **character kinematics** using modern **OpenGL**.

It combines animation blending, hierarchical transformations, and real-time input handling within a modular architecture.

---

## Authors
**Adilzhan Kurmet**  
**Adilet Malikov**

---

##  Architecture and Technology Stack
The project implements classical components of a game/graphics engine using **low-level APIs** and focuses on **real-time simulation**.

| Component | Technology / API | Description |
|-----------|-----------------|-------------|
| Programming Language | C++17 | High-performance and low-level memory control |
| Graphics API | OpenGL 3.3+ | Programmable pipeline rendering |
| Windowing & Input | GLFW | Handles keyboard input (WASD, SPACE) and window creation |
| Math Library | GLM | Matrix and vector operations (model, view, projection transformations) |
| Model Import | Assimp | Loads and processes external 3D model assets (.obj) |
| Shader Management | Custom | Vertex and fragment shaders for Phong lighting |

The architecture is modularized into:
- **Core Engine** – initializes rendering, handles the main loop, delta time, and shader management.  
- **Character Module** – processes skeletal animation and transformations.  
- **Collision System** – implements Axis-Aligned Bounding Box (AABB) collision detection.

To ensure smooth and reproducible animation and motion, **deltaTime** is used across all physics and animation calculations, maintaining frame-rate independence.

---

##  Geometry & Rendering (Rendering Pipeline)
The rendering system demonstrates the **programmable graphics pipeline** with emphasis on efficient buffer management and hierarchical transformations.

### Vertex Structure
Each vertex (`struct Vertex`) contains:
- **Position** (`glm::vec3 position`)
- **Normal** (`glm::vec3 normal`) — required for **Phong/Gouraud shading** calculations.

### Buffer Management
Implemented through the `Mesh` structure:
- **VBO** — stores vertex data (positions, normals).  
- **EBO** — holds index data for efficient `glDrawElements()` rendering.  
- **VAO** — maintains attribute bindings (position = slot 0, normal = slot 1).  

### Model Loading
External 3D models (like `.obj`) are imported using **Assimp**, ensuring compatibility with standard asset workflows used in game development.

---

##  Matrix Transformations
The rendering system uses three key transformation matrices sent to the vertex shader:

| Matrix | GLM Function | Purpose |
|--------|---------------|---------|
| Model | `glm::translate`, `glm::rotate` | Converts object (local) coordinates to world coordinates |
| View | `glm::lookAt` | Simulates the camera’s position and orientation |
| Projection | `glm::perspective` | Adds depth through perspective projection |

### Transformation Hierarchy
The system employs **hierarchical modeling**:
- Each body part (e.g., head, limbs) inherits transformations from the torso (`torsoTransform`).  
- This approach mimics a **scene graph**, allowing complex animation control over multiple linked components.

---

##  Animation and Kinematics (Procedural Animation)
The animation system is **procedural**, meaning it generates movement dynamically using **mathematical functions** instead of pre-defined keyframes.

### Limb Movement
Limb motion is driven by a sinusoidal function:

float walkCycle = std::sin(animationTime * walkCycleSpeed) * walkCycleAmplitude * movementBlend;

Blending Between States
Smooth transtions between poses (e.g., walking → running) are achieved using Linear Interpolation (LERP) via glm::mix.
Complex Poses
Leaning: For crouching or side-leaning, the torso rotation (crawlBlend * 45°) propagates through all child bones.
Jumping:
Squat Phase: Legs bend for push-off.
Apex: Legs tuck up mid-air (legTuck = jumpApexBlend * 60°).

Landing: Cushioned using a decaying velocity function to simulate impact damping.



## Physics Simulation and Collision System
Movement Physics Integration
Character movement and jumping use Euler Integration:
jumpVelocity += gravity * deltaTime;
characterHeight += jumpVelocity * deltaTime;
Gravity: -25.0f
Integration: Updates vertical position continuously for smooth arcs.
Collision Detection (AABB)
AABB (Axis-Aligned Bounding Box): Efficient box-based collision used for walls and ground.


Resolution Strategy: Trial-and-error axis separation:
Try X displacement.
Try Z displacement.
If both fail → revert to old position.
This ensures stability without jitter or penetration artifacts.
## Optimization & Performance
Single Shader Program: Reduces costly OpenGL state changes.
Batch Mesh Loading: Minimizes draw calls, improving performance.
Efficient Collision Handling: Lightweight and suitable for real-time environments.
 Average Performance: ~60+ FPS (on midrange hardware)

## Controls
KeyAction
WMove ForwardSMove BackwardARotate LeftDRotate RightSPACEJump

 Project Structure
midterm/
│
├── models/
│   └── (all obj files)
├── src/
│   └── main.cpp
└── README.md


## Build and Run Instructions
Using CMake (Recommended)
git clone <repo-url>
cd midterm
mkdir build && cd build
cmake ..
cmake --build .
./midterm

Without CMake
g++ src/main.cpp -o midterm -lglfw3 -lassimp -ldl -lGL -lX11 -lpthread -lXrandr -lXi
./midterm

(Modify library paths based on your OS setup.)

## Conclusion
The project demonstrates the core foundations of a 3D game engine:
Real-time rendering with modern OpenGL.
Hierarchical transformations and procedural animation.
Simple physics and collision mechanics.
Modular, extensible C++ structure for future expansion (camera systems, terrain, AI, etc.).

