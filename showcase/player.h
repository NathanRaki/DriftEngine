#ifndef PLAYER_H
#define PLAYER_H

// glm library
#include "ENG/includes/glm/glm.hpp"
#include "ENG/includes/glm/gtc/matrix_transform.hpp"

#include "ENG/camera/camOrt.h"
#include "ENG/camera/cam3rd.h"

#include "ENG/shaders/shader.h"
#include "ENG/model/model.h"

#include "ENG/objects/phyx.h"
#include "ENG/objects/kldr.h"
#include "ENG/objects/ctrl.h"
#include "ENG/objects/rndr.h"

const float PLAYER_SPEED = 2.5f;

enum Player_Movement {
	playerUP,
	playerDOWN,
	playerLEFT,
	playerRIGHT
};

class Player : public PhyxObj2D, public CollisionObj, public InputObj, public RenderObj
{
public:

	Cam3rd camera;
	float MovementSpeed;
	glm::vec3 Direction;

	Player(glm::vec3 position = glm::vec3(0.0f));
	//void draw(Shader *shader);
	void ProcessKeyboard(Player_Movement direction, float deltaTime);

private:

};

#endif