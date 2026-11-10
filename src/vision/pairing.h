#pragma once
#include <vector>
#include "vision/person_det.h"
#include "vision/retinaface.h"

// For each person, the index of the face paired with it (-1 = none). A face pairs when its
// center x is inside the person box and its center y is in the top 40% of the box; when
// several persons qualify, the smallest box wins. One face per person, one person per face.
std::vector<int> pair_faces(const std::vector<FaceBox>& faces, const std::vector<PersonBox>& persons);
