#ifndef __MINIBALLEVTS_HH__
#define __MINIBALLEVTS_HH__

#include <iostream>
#include <vector>
#include <string>

#include "TVector2.h"
#include "TVector3.h"
#include "TObject.h"

class GammaRayEvt : public TObject {

public:
	
	// setup functions
	GammaRayEvt() {};
	~GammaRayEvt() {};

	// Event set functions
	inline void SetEnergy( float e ){ energy = e; };
	inline void SetTime( unsigned long long t ){ time = t; };
	inline void SetClu( unsigned char c ){ clu = c; };
	inline void SetCry( unsigned char c ){ cry = c; };
	inline void SetSeg( unsigned char s ){ seg = s; };
	
	// Return functions
	inline float 				GetEnergy(){ return energy; };
	inline unsigned long long	GetTime(){ return time; };
	inline unsigned char		GetClu(){ return clu; };
	inline unsigned char		GetCry(){ return cry; };
	inline unsigned char		GetSeg(){ return seg; };

	// Geometry functions
	//inline float				GetTheta(){ return theta; };
	//inline float				GetPhi(){ return phi; };
	//TVector3					GetUnitVector(){
	//	TVector3 pos( 1, 1, 1 );
	//	pos.SetTheta( GetTheta() );
	//	pos.SetPhi( GetPhi() );
	//	return pos;
	//};

private:

	// variables for gamma-ray event
	float				energy;		///< energy in keV
	unsigned long long	time;		///< timestamp of event
	unsigned char		clu;		///< cluster ID
	unsigned char		cry;		///< crystal ID
	unsigned char		seg;		///< segment ID


	ClassDef( GammaRayEvt, 1 )

};

class GammaRayAddbackEvt : public GammaRayEvt {

public:
	
	// setup functions
	GammaRayAddbackEvt() {};
	~GammaRayAddbackEvt() {};

private:

	ClassDef( GammaRayAddbackEvt, 1 )

};



class MiniballEvts : public TObject {

public:
	
	// setup functions
	MiniballEvts() {};
	~MiniballEvts() {};
	
	void AddEvt( GammaRayEvt *event );
	void AddEvt( GammaRayAddbackEvt *event );

	inline unsigned int GetGammaRayMultiplicity(){ return gamma_event.size(); };
	inline unsigned int GetGammaRayAddbackMultiplicity(){ return gamma_ab_event.size(); };

	inline GammaRayEvt* GetGammaRayEvt( unsigned int i ){
		if( i < gamma_event.size() ) return &gamma_event.at(i);
		else return nullptr;
	};
	inline GammaRayAddbackEvt* GetGammaRayAddbackEvt( unsigned int i ){
		if( i < gamma_ab_event.size() ) return &gamma_ab_event.at(i);
		else return nullptr;
	};

	void ClearEvt();
	
	// ISOLDE timestamping
	inline void SetEBIS( unsigned long t ){ ebis = t; return; };
	inline void SetT1( unsigned long t ){ t1 = t; return; };
	
	inline unsigned long GetEBIS(){ return ebis; };
	inline unsigned long GetT1(){ return t1; };

	
private:
	
	// variables for timestamping
	unsigned long ebis;		///< absolute EBIS pulse time
	unsigned long t1;		///< absolute proton pulse time

	std::vector<GammaRayEvt> gamma_event;
	std::vector<GammaRayAddbackEvt> gamma_ab_event;

	ClassDef( MiniballEvts, 1 )
	
};

#endif
