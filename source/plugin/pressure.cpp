/******************************************************************************
 *
 * MantaFlow fluid solver framework
 * Copyright 2011 Tobias Pfaff, Nils Thuerey 
 *
 * This program is free software, distributed under the terms of the
 * GNU General Public License (GPL) 
 * http://www.gnu.org/licenses
 *
 * Plugins for pressure correction: solve_pressure, and ghost fluid helpers
 *
 ******************************************************************************/
#include "vectorbase.h"
#include "kernel.h"
#include "conjugategrad.h"

using namespace std;
namespace Manta {

//! Kernel: Construct the right-hand side of the poisson equation
KERNEL(bnd=1, reduce=+) returns(int cnt=0) returns(double sum=0)
void MakeRhs (FlagGrid& flags, Grid<Real>& rhs, MACGrid& vel, 
			  Grid<Real>* perCellCorr) 
{
	if (!flags.isFluid(i,j,k)) {
		rhs(i,j,k) = 0;
		return;
	}
	   
	// compute divergence 
	// no flag checks: assumes vel at obstacle interfaces is set to zero
	Real set =          vel(i,j,k).x - vel(i+1,j,k).x + 
						vel(i,j,k).y - vel(i,j+1,k).y; 
	if(vel.is3D()) set+=vel(i,j,k).z - vel(i,j,k+1).z;
	
	// per cell divergence correction
	if(perCellCorr) 
		set += perCellCorr->get(i,j,k);
	
	// obtain sum, cell count
	sum += set;
	cnt++;
	
	rhs(i,j,k) = set;
}

//! Kernel: Apply velocity update from poisson equation
KERNEL(bnd=1) 
void CorrectVelocity(FlagGrid& flags, MACGrid& vel, Grid<Real>& pressure) 
{
	int idx = flags.index(i,j,k);
	if (flags.isFluid(idx))
	{
		if (flags.isFluid(i-1,j,k)) vel[idx].x -= (pressure[idx] - pressure(i-1,j,k));
		if (flags.isFluid(i,j-1,k)) vel[idx].y -= (pressure[idx] - pressure(i,j-1,k));
		if (flags.is3D() && flags.isFluid(i,j,k-1)) vel[idx].z -= (pressure[idx] - pressure(i,j,k-1));
 
		if (flags.isEmpty(i-1,j,k)) vel[idx].x -= pressure[idx];
		if (flags.isEmpty(i,j-1,k)) vel[idx].y -= pressure[idx];
		if (flags.is3D() && flags.isEmpty(i,j,k-1)) vel[idx].z -= pressure[idx];
	}
	else if (flags.isEmpty(idx))
	{
		if (flags.isFluid(i-1,j,k)) vel[idx].x += pressure(i-1,j,k);
		else                        vel[idx].x  = 0.f;
		if (flags.isFluid(i,j-1,k)) vel[idx].y += pressure(i,j-1,k);
		else                        vel[idx].y  = 0.f;
		if (flags.is3D() ) {
		if (flags.isFluid(i,j,k-1)) vel[idx].z += pressure(i,j,k-1);
		else                        vel[idx].z  = 0.f;
		}
	}
}


// MLE 2014-07-02
//! Kernel: Apply velocity update from poisson equation
KERNEL(bnd=1) 
void CorrectVelocityOB(FlagGrid& flags, MACGrid& vel, Grid<Real>& pressure, Vector3D<bool> lo, Vector3D<bool> up) {

	// MLE 2014-07-02
    int idx = flags.index(i,j,k);
	bool leFluid = flags.isFluid(i-1,j,k);
	bool boFluid = flags.isFluid(i,j-1,k);
	bool frFluid = false;
	if(flags.is3D()) frFluid = flags.isFluid(i,j,k-1);

    if (flags.isFluid(idx)){
        if (leFluid)				 vel[idx].x -= (pressure[idx] - pressure(i-1,j,k));
        if (boFluid)				 vel[idx].y -= (pressure[idx] - pressure(i,j-1,k));
        if (flags.is3D() && frFluid) vel[idx].z -= (pressure[idx] - pressure(i,j,k-1));
		
        if (flags.isEmpty(i-1,j,k) || (lo.x && flags.isObstacle(i-1,j,k)))					vel[idx].x -= pressure[idx];
        if (flags.isEmpty(i,j-1,k) || (lo.y && flags.isObstacle(i,j-1,k)))					vel[idx].y -= pressure[idx];
        if (flags.is3D() && (flags.isEmpty(i,j,k-1) || (lo.z && flags.isObstacle(i,j,k-1))))	vel[idx].z -= pressure[idx];
    }
    else if (flags.isEmpty(idx)){
        if (leFluid)		vel[idx].x += pressure(i-1,j,k);
		else				vel[idx].x  = 0.f;
        if (boFluid)		vel[idx].y += pressure(i,j-1,k);
		else                vel[idx].y  = 0.f;
		if (flags.is3D()) {
			if (frFluid)	vel[idx].z += pressure(i,j,k-1);
			else            vel[idx].z  = 0.f;
		}
	}
	else if (flags.isObstacle(idx)){
		if (up.x && leFluid)		vel[idx].x += pressure(i-1,j,k);
		else if(up.x)				vel[idx].x  = 0.f;
        if (up.y && boFluid)		vel[idx].y += pressure(i,j-1,k);
		else if(up.y)				vel[idx].y  = 0.f;
		if (flags.is3D() ) {
			if (up.z && frFluid)	vel[idx].z += pressure(i,j,k-1);
			else if(up.z)		    vel[idx].z  = 0.f;
		}
    }
}



// MLE changed argument list 2014-06-30
KERNEL void SetOpenBound(Grid<Real> &A0,Grid<Real> &Ai,Grid<Real> &Aj,Grid<Real> &Ak,FlagGrid& flags,MACGrid& vel, Vector3D<bool> lo, Vector3D<bool> up) {

	if (!flags.isFluid(i,j,k))
		return;
	
	int b = flags.getBoundaryWidth();

	// set matrix stencil in and at boundary to empty
	if((lo.x && i <= b+1)||(up.x && i >= maxX-b-2)||(lo.y && j <= b+1)||(up.y && j >= maxY-b-2))
		A0(i,j,k) = (flags.is3D()) ? 6. : 4.;
	
	if ((lo.x && i <= b)||(up.x && i >= maxX-b-2))					 Ai(i,j,k) = .0;
	if ((lo.y && j <= b)||(up.y && j >= maxY-b-2))					 Aj(i,j,k) = .0;
    if (flags.is3D() && ((lo.z && k <= b)||(up.z && k >= maxZ-b-2))) Ak(i,j,k) = .0;

	// set velocity boundary conditions
	if (lo.x && i == b)				vel(b,j,k) = vel(b+1,j,k);
	if (lo.y && j == b)				vel(i,b,k) = vel(i,b+1,k);
	if (up.x && i == maxX-b-1)		vel(maxX-b-1,j,k) = vel(maxX-b-2,j,k);
	if (up.y && j == maxY-b-1)		vel(i,maxY-b-1,k) = vel(i,maxY-b-2,k);
	if(flags.is3D()) {
		if (lo.z && k == b)			vel(i,j,b) = vel(i,j,b+1);
		if (up.z && k == maxZ-b-1)	vel(i,j,maxZ-b-1) = vel(i,j,maxZ-b-2); 
	}
}


//! Kernel: Set matrix rhs for outflow
KERNEL void SetOutflow (Grid<Real>& rhs, Vector3D<bool> lowerBound, Vector3D<bool> upperBound, int height)
{
	if ((lowerBound.x && i < height) || (upperBound.x && i >= maxX-1-height) ||
		(lowerBound.y && j < height) || (upperBound.y && j >= maxY-1-height) ||
		(lowerBound.z && k < height) || (upperBound.z && k >= maxZ-1-height))
		rhs(i,j,k) = 0;
}


// *****************************************************************************
// Ghost fluid helpers

// calculate fraction filled with liquid (note, assumes inside value is < outside!)
inline static Real thetaHelper(Real inside, Real outside)
{
	Real denom = inside-outside;
	if (denom > -1e-04) return 0.5; // should always be neg, and large enough...
	return std::max(Real(0), std::min(Real(1), inside/denom));
}

// calculate ghost fluid factor, cell at idx should be a fluid cell
inline static Real ghostFluidHelper(int idx, int offset, const Grid<Real> &phi, Real gfClamp)
{
	Real alpha = thetaHelper(phi[idx], phi[idx+offset]);
	if (alpha < gfClamp) return alpha = gfClamp;
	return (1-(1/alpha)); 
}

//! Kernel: Adapt A0 for ghost fluid
KERNEL(bnd=1) 
void ApplyGhostFluidDiagonal(Grid<Real> &A0, const FlagGrid &flags, const Grid<Real> &phi, Real gfClamp)
{
	const int X = flags.getStrideX(), Y = flags.getStrideY(), Z = flags.getStrideZ();
	int idx = flags.index(i,j,k);
	if (!flags.isFluid(idx)) return;

	if (flags.isEmpty(i-1,j,k)) A0[idx] -= ghostFluidHelper(idx, -X, phi, gfClamp);
	if (flags.isEmpty(i+1,j,k)) A0[idx] -= ghostFluidHelper(idx, +X, phi, gfClamp);
	if (flags.isEmpty(i,j-1,k)) A0[idx] -= ghostFluidHelper(idx, -Y, phi, gfClamp);
	if (flags.isEmpty(i,j+1,k)) A0[idx] -= ghostFluidHelper(idx, +Y, phi, gfClamp);
	if (flags.is3D()) {
		if (flags.isEmpty(i,j,k-1)) A0[idx] -= ghostFluidHelper(idx, -Z, phi, gfClamp);
		if (flags.isEmpty(i,j,k+1)) A0[idx] -= ghostFluidHelper(idx, +Z, phi, gfClamp);
	}
}

//! Kernel: Apply velocity update: ghost fluid contribution
KERNEL(bnd=1)
void CorrectVelocityGhostFluid(MACGrid &vel, const FlagGrid &flags, const Grid<Real> &pressure, const Grid<Real> &phi, Real gfClamp)
{
	const int X = flags.getStrideX(), Y = flags.getStrideY(), Z = flags.getStrideZ();
	const int idx = flags.index(i,j,k);
	if (flags.isFluid(idx))
	{
		if (flags.isEmpty(i-1,j,k)) vel[idx][0] += pressure[idx] * ghostFluidHelper(idx, -X, phi, gfClamp);
		if (flags.isEmpty(i,j-1,k)) vel[idx][1] += pressure[idx] * ghostFluidHelper(idx, -Y, phi, gfClamp);
		if (flags.is3D() && flags.isEmpty(i,j,k-1)) vel[idx][2] += pressure[idx] * ghostFluidHelper(idx, -Z, phi, gfClamp);
	}
	else if (flags.isEmpty(idx))
	{
		if (flags.isFluid(i-1,j,k)) vel[idx][0] -= pressure(i-1,j,k) * ghostFluidHelper(idx-X, +X, phi, gfClamp);
		else                        vel[idx].x  = 0.f;
		if (flags.isFluid(i,j-1,k)) vel[idx][1] -= pressure(i,j-1,k) * ghostFluidHelper(idx-Y, +Y, phi, gfClamp);
		else                        vel[idx].y  = 0.f;
		if (flags.is3D() ) {
		if (flags.isFluid(i,j,k-1)) vel[idx][2] -= pressure(i,j,k-1) * ghostFluidHelper(idx-Z, +Z, phi, gfClamp);
		else                        vel[idx].z  = 0.f;
		}
	}
}


// improve behavior of clamping for large time steps:

inline static Real ghostFluidWasClamped(int idx, int offset, const Grid<Real> &phi, Real gfClamp)
{
	Real alpha = thetaHelper(phi[idx], phi[idx+offset]);
	if (alpha < gfClamp) return true;
	return false;
}

KERNEL(bnd=1)
void ReplaceClampedGhostFluidVels(MACGrid &vel, FlagGrid &flags, 
		const Grid<Real> &pressure, const Grid<Real> &phi, Real gfClamp )
{
	const int X = flags.getStrideX(), Y = flags.getStrideY(), Z = flags.getStrideZ();
	const int idx = flags.index(i,j,k);
	if (flags.isFluid(idx))
	{
		if( (flags.isEmpty(i-1,j,k)) && (ghostFluidWasClamped(idx, -X, phi, gfClamp)) )
			vel[idx-X][0] = vel[idx][0];
		if( (flags.isEmpty(i,j-1,k)) && (ghostFluidWasClamped(idx, -Y, phi, gfClamp)) )
			vel[idx-Y][1] = vel[idx][1];
		if( flags.is3D() && 
		   (flags.isEmpty(i,j,k-1)) && (ghostFluidWasClamped(idx, -Z, phi, gfClamp)) )
			vel[idx-Z][2] = vel[idx][2];
	}
	else if (flags.isEmpty(idx))
	{
		if( (i>-1) && (flags.isFluid(i-1,j,k)) && ( ghostFluidWasClamped(idx-X, +X, phi, gfClamp) ) )
			vel[idx][0] = vel[idx-X][0];
		if( (j>-1) && (flags.isFluid(i,j-1,k)) && ( ghostFluidWasClamped(idx-Y, +Y, phi, gfClamp) ) )
			vel[idx][1] = vel[idx-Y][1];
		if( flags.is3D() &&
		  ( (k>-1) && (flags.isFluid(i,j,k-1)) && ( ghostFluidWasClamped(idx-Z, +Z, phi, gfClamp) ) ))
			vel[idx][2] = vel[idx-Z][2];
	}
}


// *****************************************************************************
// Main pressure solve

inline void convertDescToVec(const string& desc, Vector3D<bool>& lo, Vector3D<bool>& up) {
	for(size_t i=0; i<desc.size(); i++) {
		if (desc[i] == 'x') lo.x = true;
		else if (desc[i] == 'y') lo.y = true;
		else if (desc[i] == 'z') lo.z = true;
		else if (desc[i] == 'X') up.x = true;
		else if (desc[i] == 'Y') up.y = true;
		else if (desc[i] == 'Z') up.z = true;
		else errMsg("invalid character in boundary description string. Only [xyzXYZ] allowed.");
	}
}

// MLE 2014-06-30 changed position of openBound for calling solvePressure from c++
//! Perform pressure projection of the velocity grid
PYTHON void solvePressure(MACGrid& vel, Grid<Real>& pressure, FlagGrid& flags, string openBound="",
                     Grid<Real>* phi = 0, 
                     Grid<Real>* perCellCorr = 0, 
                     Real gfClamp = 1e-04,
                     Real cgMaxIterFac = 1.5,
                     Real cgAccuracy = 1e-3,
                     string outflow = "",
                     int outflowHeight = 1,
                     bool precondition = true,
                     bool enforceCompatibility = false,
                     bool useResNorm = true )
{
	// parse strings
	Vector3D<bool> loOpenBound, upOpenBound, loOutflow, upOutflow;
	convertDescToVec(openBound, loOpenBound, upOpenBound);
	convertDescToVec(outflow, loOutflow, upOutflow);
	if (vel.is2D() && (loOpenBound.z || upOpenBound.z))
		errMsg("open boundaries for z specified for 2D grid");
	
	// reserve temp grids
	FluidSolver* parent = flags.getParent();
	Grid<Real> rhs(parent);
	Grid<Real> residual(parent);
	Grid<Real> search(parent);
	Grid<Real> A0(parent);
	Grid<Real> Ai(parent);
	Grid<Real> Aj(parent);
	Grid<Real> Ak(parent);
	Grid<Real> tmp(parent);
	Grid<Real> pca0(parent);
	Grid<Real> pca1(parent);
	Grid<Real> pca2(parent);
	Grid<Real> pca3(parent);
		
	// setup matrix and boundaries
	MakeLaplaceMatrix (flags, A0, Ai, Aj, Ak);
	// MLE 2014-06-30 changed vel parameter to flags parameter
	SetOpenBound (A0, Ai, Aj, Ak, flags, vel, loOpenBound, upOpenBound);
	
	if (phi) {
		ApplyGhostFluidDiagonal(A0, flags, *phi, gfClamp);
	}
	
	// compute divergence and init right hand side
	MakeRhs kernMakeRhs (flags, rhs, vel, perCellCorr);
	
	if (!outflow.empty())
		SetOutflow (rhs, loOutflow, upOutflow, outflowHeight);
	
	if (enforceCompatibility)
		rhs += (Real)(-kernMakeRhs.sum / (Real)kernMakeRhs.cnt);
	
	// CG setup
	// note: the last factor increases the max iterations for 2d, which right now can't use a preconditioner 
	const int maxIter = (int)(cgMaxIterFac * flags.getSize().max()) * (flags.is3D() ? 1 : 4);
	GridCgInterface *gcg;
	if (vel.is3D())
		gcg = new GridCg<ApplyMatrix>(pressure, rhs, residual, search, flags, tmp, &A0, &Ai, &Aj, &Ak );
	else
		gcg = new GridCg<ApplyMatrix2D>(pressure, rhs, residual, search, flags, tmp, &A0, &Ai, &Aj, &Ak );
	
	gcg->setAccuracy( cgAccuracy ); 
	gcg->setUseResNorm( useResNorm );

	// optional preconditioning
	gcg->setPreconditioner( precondition ? GridCgInterface::PC_mICP : GridCgInterface::PC_None, &pca0, &pca1, &pca2, &pca3);

	for (int iter=0; iter<maxIter; iter++) {
		if (!gcg->iterate()) iter=maxIter;
	} 
	debMsg("FluidSolver::solvePressure iterations:"<<gcg->getIterations()<<", res:"<<gcg->getSigma(), 1);
	delete gcg;
	
	// MLE 2014-06-30 add parameters loOpenBound and upOpenBound
	//CorrectVelocityOB(flags, vel, pressure, loOpenBound, upOpenBound); 
	CorrectVelocity(flags, vel, pressure ); 
	if (phi) {
		CorrectVelocityGhostFluid (vel, flags, pressure, *phi, gfClamp);
		// improve behavior of clamping for large time steps:
		ReplaceClampedGhostFluidVels (vel, flags, pressure, *phi, gfClamp);
	}
}

} // end namespace

