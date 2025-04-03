cmake_minimum_required(VERSION 3.5)
project(PastecExamples)

# Find Pastec package
find_package(Pastec REQUIRED)

# Find required packages
find_package(OpenCV REQUIRED)

# Add the example executable
add_executable(simple_index simple_index.cpp)

# Link against Pastec library
target_link_libraries(simple_index Pastec::pastec ${OpenCV_LIBS})