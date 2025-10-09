# Compgrafmidka

# Character Control with WASD and Biped Skeletal Animation

## Course

Computer Graphics Fundamentals

## Project Description

This project is a prototype of a 3D character controller using **WASD movement keys** and a **biped skeletal animation**. The goal is to demonstrate basic character movement and rotation in a 3D environment using **OpenGL**, **GLFW**, **GLM**, and **Assimp**.

### Features

* Move character forward/backward/left/right with **WASD**
* Rotate character left/right
* Simple jump with **SPACE** key
* Basic 3D rendering of a biped model
* Fixed camera view

## Screenshots

im lazy

## Project Structure

```
midterm/
│
├── models/
│   └── biped.obj
├── src/
│   └── main.cpp
├── CMakeLists.txt (optional)
└── README.md
```

## Requirements

* C++17 compatible compiler
* [GLFW](https://www.glfw.org/)
* [GLAD](https://glad.dav1d.de/)
* [GLM](https://glm.g-truc.net/)
* [Assimp](https://www.assimp.org/)
* OpenGL 3.3 or higher

## Build and Run Instructions

### Using CMake (recommended)

1. Clone the repository



2. Create a build folder and navigate to it:


mkdir build && cd build


3. Run CMake to configure the project:


cmake ..


4. Build the project:


cmake --build .


5. Run the executable:


./midterm


### Without CMake

1. Ensure all dependencies (GLFW, GLAD, GLM, Assimp) are installed and linked correctly.
2. Compile with a command similar to:


g++ src/main.cpp -o midterm -lglfw3 -lassimp -ldl -lGL -lX11 -lpthread -lXrandr -lXi


*(Modify according to your OS and library paths.)*
3. Run the executable:


./midterm


## Controls

* **W** – Move forward
* **S** – Move backward
* **A** – Rotate left
* **D** – Rotate right
* **SPACE** – Jump

## Notes

* The camera is fixed in a static position.
* The project uses a single biped model (`biped.obj`) located in the `models/` folder. Make sure the path is correct in the code if moved.

