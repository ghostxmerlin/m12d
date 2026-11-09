#pragma once
#include <cstdint>

// Five-point ArcFace alignment to a 112x112 RGB crop (template x shifted by +8, as in the
// stock arcface.cpp). Least-squares similarity via ncnn::get_affine_transform.
//
// tm maps image coordinates to the 112x112 template: x' = tm0*x + tm1*y + tm2, y' = tm3*x + tm4*y + tm5.
void similarity_to_template(const float lm[5][2], float tm[6]);
// rgb: packed RGB w x h; lm in the same coordinates; out: 112*112*3 bytes.
void align_face(const uint8_t* rgb, int w, int h, const float lm[5][2], uint8_t* out112);
