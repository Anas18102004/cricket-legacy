#ifndef TEST_D3D8_SCENE_H
#define TEST_D3D8_SCENE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initializes the 3D D3D8 test scene (geometry, textures, shaders, camera)
bool test_d3d8_scene_init(void);

// Renders one animated frame through the Direct3D 8 pipeline
void test_d3d8_scene_render_frame(void);

// Releases test scene resources
void test_d3d8_scene_shutdown(void);

// Queries frame count rendered by test scene
uint32_t test_d3d8_scene_get_frame_count(void);

#ifdef __cplusplus
}
#endif

#endif // TEST_D3D8_SCENE_H
