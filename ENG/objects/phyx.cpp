#include "phyx.h"
#include <iostream>
static const int PHYX_LAYER=0;

void PhyxENG::Init(std::vector<GameObj*>* gameobjects, CollisionENG *ce,SoundENG *se){
	managed.clear();
	for (unsigned int i = 0; i < gameobjects->size(); i++)
	{
		GameObj* go = gameobjects->at(i);
		PhyxObj2D* cast = dynamic_cast<PhyxObj2D *>(go);
		if(cast) managed.push_back(cast);
	}
	collisionENG = ce;
	soundENG = se;
}

void PhyxENG::Update(){
//	TESTLOG("PhyxENG::Update");
	auto tn = std::chrono::steady_clock::now();
	std::chrono::duration<double, std::milli> d = tn - t;
	double dd = (d.count()/1000)*timescale;
	t=tn;
	timecounter+=dd;
	if(timecounter>1.){
		fps = framecounter;
		framecounter = 0;
		timecounter-=1.;
	}
	framecounter++;

	int cols=0;
	for(int i=0;i<managed.size();i++){
			PhyxObj2D * p = managed[i];
		for(int j=i+1;j<managed.size();j++){ //this kind of loop allows us to only check a couple once
			PhyxObj2D * q = managed[j];
			
			//Global Forces between all objects
			//maybe give the PhyxENG settings to toggle these
			glm::dvec2 g = PhyxENG::Gravity2D(p,q);
			if(p->parent==nullptr){
			if(gravitymode == Everything){
				TESTLOG("PhyxENG::Update Gravity Between" TAB p->name TAB q->name TAB glm::length(g));
				if(!p->isKinematic()) p->AddForce(g);
				if(!q->isKinematic()) q->AddForce(-g);
			} else if(gravitymode == Orbiting){
				TESTLOG("PhyxENG::Update Gravity Between" TAB p->name TAB q->name TAB glm::length(g));
				if(p->Orbiting(q) && !p->isKinematic()) p->AddForce(g);
				if(q->Orbiting(p) && !q->isKinematic()) q->AddForce(-g);
			} else if (gravitymode == Directional){
				p->AddForce(glm::dvec2(0,-1)*p->mass);
			}
			}

			CollisionMsg * pqData = collisionENG->CollisionBetween(p,q,PHYX_LAYER);
			if(pqData && clipping){
				TESTLOG("PhyxENG::Update collision detected" TAB p->name TAB q->name);
				cols++;
				if(soundENG && glm::length(glm::dot(p->v,q->v))>0.1){
						soundENG->Play(2, false);
					}
//				if(!p->isKinematic()) p->AddForce(-g);
//				if(!q->isKinematic()) q->AddForce(g);
//				if(p->orbiting = q) p->AddForce(-g);
				TESTLOG("p mass:" TAB p->Mass() TAB "q mass" TAB q->Mass() TAB p->Mass()-q->Mass());
				TESTLOG("p speed:" TAB p->Speed() TAB "q speed" TAB q->Speed() TAB glm::dot(p->V(),q->V()));
				StaticResolution(p,pqData->P.second, q,pqData->Q.second);
				DynamicResolution(p,pqData->P.second, q,pqData->Q.second);
				TESTLOG("p speed:" TAB p->Speed() TAB "q speed" TAB q->Speed() TAB glm::dot(p->V(),q->V()));
			}
		}
		//now that i has seen all the other phyxObj, he's finished working
		if(!p->isKinematic()) p->Update(dd);
		p->ResetA();
	}
	if(cols) TESTLOG("collisions managed" TAB cols);
//	 std::cout<<"collision count"<<cols<<std::endl;
}

void PhyxENG::StaticResolution(Collider *p,Collider *q){
//	TESTLOG("PhyxENG::StaticResolution");
	glm::dvec2 p2q = p->worldPosition2D() - q->worldPosition2D();
	glm::dvec2 nor = glm::normalize(p2q);

	CircleCollider *pc = dynamic_cast<CircleCollider *>(p);
	CircleCollider *qc = dynamic_cast<CircleCollider *>(q);
	if(pc&&qc){
		double overlap = (pc->Dim() + qc->Dim())-glm::length(p2q);
		p->Move(nor * overlap*.5);
		q->Move(nor*-1. * overlap*.5);
	} else {
		//at least try
		p->Move(nor);
		q->Move(nor*-1.);
	}

}
void PhyxENG::StaticResolution(PhyxObj2D* p, Collider * pc,PhyxObj2D*q, Collider *qc){
	TESTLOG("PhyxENG::StaticResolution" TAB p->name TAB q->name);
	glm::dvec2 p2q = pc->worldPosition2D() - qc->worldPosition2D();
	glm::dvec2 nor = glm::normalize(p2q);
	double p2qMassRatio = p->mass / (p->mass+q->mass);
	double q2pMassRatio = q->mass / (p->mass+q->mass);

	CircleCollider *pcc = dynamic_cast<CircleCollider *>(pc);
	CircleCollider *qcc = dynamic_cast<CircleCollider *>(qc);
	if(pcc&&qcc){
		double overlap = ((pcc->Dim() + qcc->Dim())-glm::length(p2q))*1.01;
		if(!p->isKinematic() && !q->isKinematic()){
//			TESTLOG("normal collision");
			p->Move(nor * overlap * q2pMassRatio);
			q->Move(nor*-1. * overlap * p2qMassRatio);
		}
		else if((p->isKinematic() || q->Orbiting(p)) && !q->isKinematic()){
			q->Move(nor*-1. * overlap);
		}
		else if(!p->isKinematic() && (q->isKinematic()|| p->Orbiting(q))){
			p->Move(nor * overlap);
		} else {
			//what happens an unstoppable force
			//meets and immovable object ? 
		}
	} else {
		//at least try
		p->Move(nor);
		q->Move(nor*-1.);
	}
}

void PhyxENG::DynamicResolution(PhyxObj2D* p, Collider * pc,PhyxObj2D*q, Collider *qc){
	TESTLOG("PhyxENG::DynamicResolution" TAB p->name TAB q->name);
	glm::dvec2 p2q = pc->worldPosition2D() - qc->worldPosition2D();
	glm::dvec2 nor = glm::normalize(p2q);
	TESTLOG(p->name<<"Collider(x,y)" TAB pc->worldPosition2D().x TAB pc->worldPosition2D().y);
	TESTLOG(q->name<<"Collider(x,y)" TAB qc->worldPosition2D().x TAB qc->worldPosition2D().y);
	TESTLOG("Collision Normal (x,y)->" TAB nor.x TAB nor.y);


	CircleCollider *pcc = dynamic_cast<CircleCollider *>(pc);
	CircleCollider *qcc = dynamic_cast<CircleCollider *>(qc);
	if(pcc&&qcc){
		TESTLOG("Circle Resolution");
		glm::dvec2 tan = glm::vec2(nor.y*-1.,nor.x);
		TESTLOG("Collision Tangent (x,y)->" TAB tan.x TAB tan.y);
//		if(!p->isKinematic() && !q->isKinematic()){
			
			double pdottan = glm::dot(p->V(),tan);
			double qdottan = glm::dot(q->V(),tan);
			double pdotnor = glm::dot(p->V(),nor);
			double qdotnor = glm::dot(q->V(),nor);

			double pmomentum = (pdotnor*(p->mass - q->mass) + 2.*q->mass*qdotnor)/(p->mass+q->mass);
			double qmomentum = (qdotnor*(q->mass - p->mass) + 2.*p->mass*pdotnor)/(p->mass+q->mass);

			TESTLOG(p->name<<"NormalMomentum(x,y)->" TAB pmomentum);
			TESTLOG(q->name<<"NormalMomentum(x,y)->" TAB qmomentum);

			if(!p->isKinematic()) p->V((tan*pdottan + nor*pmomentum)*colEl);
			if(!q->isKinematic()) q->V((tan*qdottan + nor*qmomentum)*colEl);

/*		} else if((p->isKinematic() || q->orbiting==p) && !q->isKinematic()){
			double dot = glm::dot(tan,q->V());
			q->XV(tan.x*dot*colEl);
			q->YV(tan.y*dot*colEl);

		} else if(!p->isKinematic() && (q->isKinematic()|| p->orbiting==q)){
			double dot = glm::dot(tan,p->V());
			p->XV(tan.x*dot*colEl);
			p->YV(tan.y*dot*colEl);
		}*/

	} else {
		//at least try
		p->ResetV();
		q->ResetV();
		p->ResetA();
		q->ResetA();
	}
}

glm::dvec2 PhyxENG::Gravity2D(PhyxObj2D* a,PhyxObj2D* b) {
	glm::dvec2 a2b = b->worldPosition2D() - a->worldPosition2D();
//	float G = ;//6.67408/100000000000.;
	double Mm = a->Mass()*b->Mass();
	double d = glm::length(a2b);
	double r2 = (d*d)/4.f;
	return a2b*(G*(Mm/r2));
}

PhyxObj2D::PhyxObj2D()
{
	a = glm::dvec2(0.0f);
	v = glm::dvec2(0.0f);
	mass = 1.0f;
}


void PhyxObj2D::Update(double dt){
	dV(a * dt);
	if(!parent) Move(v*dt);
//	if(Speed()<0.00001) v=glm::vec2(0,0);
}
void PhyxObj2D::ResetA(){a=glm::vec2(0);}
void PhyxObj2D::ResetV(){v=glm::vec2(0);}
void PhyxObj2D::AddForce(glm::dvec2 _a) {
	a+=_a/mass;
}

glm::dvec2		PhyxObj2D::V(){
	if(parent) {
		auto p = dynamic_cast<PhyxObj2D*>(parent);
		if(p) return p->V();
		else return v;
	}
	else return v;
}
void			PhyxObj2D::V(glm::dvec2 _v){
	if(parent) {
		auto p = dynamic_cast<PhyxObj2D*>(parent);
		if(p) p->V(_v);
	}
	else v=_v;
}
void			PhyxObj2D::dV(glm::dvec2 _v){
	if(parent) {
		auto p = dynamic_cast<PhyxObj2D*>(parent);
		if(p) p->dV(_v);
	}
	else v+=_v;
}
float const 	PhyxObj2D::XV(){
	if(parent) {
		auto p = dynamic_cast<PhyxObj2D*>(parent);
		if(p) return p->XV();
		else return v.x;
	}
	else return v.x;
}
void 			PhyxObj2D::XV(float _xv){
	if(parent) {
		auto p = dynamic_cast<PhyxObj2D*>(parent);
		if(p) p->XV(_xv);
	}
	else v.x=_xv;
}
float const 	PhyxObj2D::YV(){
	if(parent) {
		auto p = dynamic_cast<PhyxObj2D*>(parent);
		if(p) return p->YV();
		else return v.y;
	}
	else return v.y;
}
void 			PhyxObj2D::YV(float _yv){
	if(parent) {
		auto p = dynamic_cast<PhyxObj2D*>(parent);
		if(p) p->YV(_yv);
	}
	else v.y=_yv;
}
float 			PhyxObj2D::Speed(){
	if(parent) {
		auto p = dynamic_cast<PhyxObj2D*>(parent);
		if(p) return p->Speed();
		else return glm::length(v);
	}
	else return glm::length(v);
}