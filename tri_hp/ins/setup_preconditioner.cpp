#include <math.h>

#include "tri_hp_ins.h"
#include "../hp_boundary.h"

// #define TIMEACCURATE
#define REFINED_WAY

int tri_hp_ins::setup_preconditioner() {
	TinyVector<FLT,ND> mvel;
    int err = 0;

	if (hp_gbl->diagonal_preconditioner) {
		/* SET-UP DIAGONAL PRECONDITIONER */
        int tind,i,j,side,err=0;
		FLT jcb,h,hmax,q,qmax,lam1,gam;
		TinyVector<int,3> v;

		FLT nu = hp_ins_gbl->mu/hp_ins_gbl->rho;
		
		/***************************************/
		/** DETERMINE FLOW PSEUDO-TIME STEP ****/
		/***************************************/
		hp_gbl->vprcn(Range(0,npnt-1),Range::all()) = 0.0;
		if (basis::tri(log2p)->sm() > 0) {
			hp_gbl->sprcn(Range(0,nseg-1),Range::all()) = 0.0;
		}

#ifdef TIMEACCURATE
		gam = 100.0;
		FLT dtstari = 0.0;
#endif

		for(tind = 0; tind < ntri; ++tind) {
			jcb = 0.25*area(tind);  // area is 2 x triangle area
			v = tri(tind).pnt;
			
#ifdef REFINED_WAY
			/* LOAD SOLUTION COEFFICIENTS FOR THIS ELEMENT */
			/* PROJECT SOLUTION TO GAUSS POINTS WITH DERIVATIVES IF NEEDED FOR VISCOUS TERMS */
			ugtouht(tind);
			for(int n=0;n<ND;++n)
				basis::tri(log2p)->proj(&uht(n)(0),&u(n)(0,0),MXGP);
				
			/* IF TINFO > -1 IT IS CURVED ELEMENT */
			if (tri(tind).info > -1) {
				/* LOAD ISOPARAMETRIC MAPPING COEFFICIENTS */
				crdtocht(tind);

				/* PROJECT COORDINATES AND COORDINATE DERIVATIVES TO GAUSS POINTS */
				for(int n=0;n<ND;++n)
					basis::tri(log2p)->proj_bdry(&cht(n,0), &crd(n)(0,0), &dcrd(n,0)(0,0), &dcrd(n,1)(0,0),MXGP);
			
				qmax = 0.0;
				hmax = 0.0;
				FLT jcbmin = jcb;
				int lgpx = basis::tri(log2p)->gpx(), lgpn = basis::tri(log2p)->gpn();
				for(i=0;i<lgpx;++i) {
					for(j=0;j<lgpn;++j) {
					
						mvel(0) = gbl->bd(0)*(crd(0)(i,j) -dxdt(log2p)(tind,0,i,j));
						mvel(1) = gbl->bd(0)*(crd(1)(i,j) -dxdt(log2p)(tind,1,i,j));
#ifdef MESH_REF_VEL
						mvel(0) += hp_gbl->mesh_ref_vel(0);
						mvel(1) += hp_gbl->mesh_ref_vel(1);
#endif                        
						jcbmin = MIN(jcbmin,dcrd(0,0)(i,j)*dcrd(1,1)(i,j) -dcrd(1,0)(i,j)*dcrd(0,1)(i,j));
						
						/* CALCULATE CURVED SIDE LENGTHS */
						h = 0.0;
						for (int n=0;n<ND;++n)
							h += dcrd(n,0)(i,j)*dcrd(n,0)(i,j);
						hmax = MAX(h,hmax);
							
						h = 0.0;
						for (int n=0;n<ND;++n)
							h += dcrd(n,1)(i,j)*dcrd(n,1)(i,j);
						hmax = MAX(h,hmax);

						h = 0.0;
						for (int n=0;n<ND;++n)
							h += (dcrd(n,1)(i,j) -dcrd(n,0)(i,j))*(dcrd(n,1)(i,j) -dcrd(n,0)(i,j));
						hmax = MAX(h,hmax);
					
						q = pow(u(0)(i,j)-0.5*mvel(0),2.0)  +pow(u(1)(i,j)-0.5*mvel(1),2.0);
						qmax = MAX(qmax,q);
					}
				}	
				hmax = 2.*sqrt(hmax);
				h = 4.*jcbmin/(0.25*(basis::tri(log2p)->p() +1)*(basis::tri(log2p)->p()+1)*hmax);
				hmax = hmax/(0.25*(basis::tri(log2p)->p() +1)*(basis::tri(log2p)->p()+1));
			}
			else {
				/* PROJECT VERTEX COORDINATES AND COORDINATE DERIVATIVES TO GAUSS POINTS */
				for(int n=0;n<ND;++n)
					basis::tri(log2p)->proj(pnts(v(0))(n),pnts(v(1))(n),pnts(v(2))(n),&crd(n)(0,0),MXGP);
			
				qmax = 0.0;
				hmax = 0.0;
				int lgpx = basis::tri(log2p)->gpx(), lgpn = basis::tri(log2p)->gpn();
				for(i=0;i<lgpx;++i) {
					for(j=0;j<lgpn;++j) {
						
						mvel(0) = gbl->bd(0)*(crd(0)(i,j) -dxdt(log2p)(tind,0,i,j));
						mvel(1) = gbl->bd(0)*(crd(1)(i,j) -dxdt(log2p)(tind,1,i,j));
#ifdef MESH_REF_VEL
						mvel(0) += hp_gbl->mesh_ref_vel(0);
						mvel(1) += hp_gbl->mesh_ref_vel(1);
#endif                        
						q = pow(u(0)(i,j)-0.5*mvel(0),2.0)  +pow(u(1)(i,j)-0.5*mvel(1),2.0);
						qmax = MAX(qmax,q);
					
					}
				}
				for(j=0;j<3;++j) {
					h = pow(pnts(v(j))(0) -pnts(v((j+1)%3))(0),2.0) +pow(pnts(v(j))(1) -pnts(v((j+1)%3))(1),2.0);
					hmax = (h > hmax ? h : hmax);
				}
				hmax = sqrt(hmax);
				h = 4.*jcb/(0.25*(basis::tri(log2p)->p() +1)*(basis::tri(log2p)->p()+1)*hmax);
				hmax = hmax/(0.25*(basis::tri(log2p)->p() +1)*(basis::tri(log2p)->p()+1));
			}

#else
			qmax = 0.0;
			hmax = 0.0;
			for(j=0;j<3;++j) {
				h = pow(pnts(v(j))(0) -pnts(v((j+1)%3))(0),2.0) +pow(pnts(v(j))(1) -pnts(v((j+1)%3))(1),2.0);
				hmax = (h > hmax ? h : hmax);

				v0 = v(j);
				
				mvel(0) = gbl->bd(0)*(pnts(v0)(0) -vrtxbd(1)(v0)(0));
				mvel(1) = gbl->bd(0)*(pnts(v0)(1) -vrtxbd(1)(v0)(1));
#ifdef MESH_REF_VEL
				mvel += hp_gbl->mesh_ref_vel;
#endif
				
				q = pow(ug.v(v0,0)-0.5*mvel(0),2.0) +pow(ug.v(v0,1)-0.5*mvel(1),2.0);

				qmax = MAX(qmax,q);
			}
			hmax = sqrt(hmax);
			h = 4.*jcb/(0.25*(basis::tri(log2p)->p() +1)*(basis::tri(log2p)->p()+1)*hmax);
			hmax = hmax/(0.25*(basis::tri(log2p)->p() +1)*(basis::tri(log2p)->p()+1));
#endif
			

			if (!(h > 0.0)) { 
				*gbl->log << "negative triangle area caught in tstep. Problem triangle is : " << tind << std::endl;
				*gbl->log << "approximate location: " << pnts(v(0))(0) << ' ' << pnts(v(0))(1) << std::endl;
                err = 1;
                break;
			}

			if  (std::isnan(qmax)) { 
				*gbl->log << gbl->idprefix << ' ' << tind << std::endl;
				*gbl->log << "flow solution has nan's " << qmax << std::endl;
                err = 1;
                break;
			}

#ifndef INERTIALESS

#ifndef TIMEACCURATE
			gam = 3.0*qmax +(0.5*hmax*gbl->bd(0) +2.*nu/hmax)*(0.5*hmax*gbl->bd(0) +2.*nu/hmax);
			if (hp_ins_gbl->mu + gbl->bd(0) == 0.0) gam = MAX(gam,0.1);
#endif
			q = sqrt(qmax);
			lam1 = q + sqrt(qmax +gam);

			/* SET UP DISSIPATIVE COEFFICIENTS */
			hp_ins_gbl->tau(tind,0) = adis*h/(jcb*sqrt(gam));
			hp_ins_gbl->tau(tind,NV-1) = qmax*hp_ins_gbl->tau(tind,0);

			/* SET UP DIAGONAL PRECONDITIONER */
			// jcb *= 8.*nu*(1./(hmax*hmax) +1./(h*h)) +2*lam1/h +2*sqrt(gam)/hmax +gbl->bd(0);
			jcb *= 2.*nu*(1./(hmax*hmax) +1./(h*h)) +3*lam1/h;  // heuristically tuned
#else
			gam = pow(2.*nu/hmax,2); 
			lam1 = sqrt(gam);

			/* SET UP DISSIPATIVE COEFFICIENTS */
			hp_ins_gbl->tau(tind,0)  = adis*h/(jcb*sqrt(gam));
			hp_ins_gbl->tau(tind,NV-1) = 0.0;

			jcb *= 8.*nu*(1./(hmax*hmax) +1./(h*h)) +2*lam1/h +2*sqrt(gam)/hmax;
#endif
#ifdef TIMEACCURATE
			dtstari = MAX((nu/(h*h) +lam1/h +gbl->bd(0)),dtstari);

		}
		*gbl->log << "#iterative to physical time step ratio: " << gbl->bd(0)/dtstari << '\n';

		for(tind=0;tind<ntri;++tind) {
			v = tri(tind).pnt;
			jcb = 0.25*area(tind)*dtstari;
#endif

			jcb *= RAD((pnts(v(0))(0) +pnts(v(1))(0) +pnts(v(2))(0))/3.);

			hp_gbl->tprcn(tind,0) = hp_ins_gbl->rho*jcb;   
			hp_gbl->tprcn(tind,1) = hp_ins_gbl->rho*jcb;   
			hp_gbl->tprcn(tind,2) = jcb/gam; 			

			/* BRUTE FORCE WAY TO FREEZE FLOW */
//			hp_gbl->tprcn(tind,0) = 1.0e99*hp_ins_gbl->rho*jcb;   
//			hp_gbl->tprcn(tind,1) = 1.0e99*hp_ins_gbl->rho*jcb;   
//			hp_gbl->tprcn(tind,2) =  1.0e99*jcb/gam;
			


			for(i=0;i<3;++i) {
				hp_gbl->vprcn(v(i),Range::all())  += hp_gbl->tprcn(tind,Range::all());
				if (basis::tri(log2p)->sm() > 0) {
					side = tri(tind).seg(i);
					hp_gbl->sprcn(side,Range::all()) += hp_gbl->tprcn(tind,Range::all());
				}
			}
		}
	}
	else {
		/* SET-UP MATRIX PRECONDITIONER */
		int tind,i,j,side,v0;
		FLT jcb,h,hmax,q,qmax,lam1,gam,ubar,vbar;
		TinyVector<int,3> v;

		FLT nu = hp_ins_gbl->mu/hp_ins_gbl->rho;

		/***************************************/
		/** DETERMINE FLOW PSEUDO-TIME STEP ****/
		/***************************************/
		hp_gbl->vprcn_ut(Range(0,npnt-1),Range::all(),Range::all()) = 0.0;
		if (basis::tri(log2p)->sm() > 0) {
			hp_gbl->sprcn_ut(Range(0,nseg-1),Range::all(),Range::all()) = 0.0;
		}
		hp_gbl->tprcn_ut(Range(0,ntri-1),Range::all(),Range::all()) = 0.0;


		for(tind = 0; tind < ntri; ++tind) {
			jcb = 0.25*area(tind);  // area is 2 x triangle area
			v = tri(tind).pnt;
			hmax = 0.0;
			for(j=0;j<3;++j) {
				h = pow(pnts(v(j))(0) -pnts(v((j+1)%3))(0),2.0) + 
				pow(pnts(v(j))(1) -pnts(v((j+1)%3))(1),2.0);
				hmax = (h > hmax ? h : hmax);
			}
			hmax = sqrt(hmax);

			if (!(jcb > 0.0)) {  // THIS CATCHES NAN'S TOO
				*gbl->log << "negative triangle area caught in tstep. Problem triangle is : " << tind << std::endl;
				*gbl->log << "approximate location: " << pnts(v(0))(0) << ' ' << pnts(v(0))(1) << std::endl;
                err = 1;
                break;
			}
			h = 4.*jcb/(0.25*(basis::tri(log2p)->p() +1)*(basis::tri(log2p)->p()+1)*hmax);
			hmax = hmax/(0.25*(basis::tri(log2p)->p() +1)*(basis::tri(log2p)->p()+1));

			qmax = 0.0;
			ubar = 0.0;
			vbar = 0.0;
			for(j=0;j<3;++j) {
				v0 = v(j);
				mvel(0) = gbl->bd(0)*(pnts(v0)(0) -vrtxbd(1)(v0)(0));
				mvel(1) = gbl->bd(0)*(pnts(v0)(1) -vrtxbd(1)(v0)(1));
#ifdef MESH_REF_VEL
				mvel += hp_gbl->mesh_ref_vel;
#endif
				q = pow(ug.v(v0,0) -mvel(0),2.0) +pow(ug.v(v0,1) -mvel(1),2.0);  

				ubar += ug.v(v0,0);
				vbar += ug.v(v0,1);
				qmax = MAX(qmax,q);
			}

			ubar /= 3.0;
			vbar /= 3.0;

			gam = 3.0*qmax +(0.5*hmax*gbl->bd(0) +2.*nu/hmax)*(0.5*hmax*gbl->bd(0) +2.*nu/hmax);
			if (hp_ins_gbl->mu + gbl->bd(0) == 0.0) gam = MAX(gam,0.1);

			q = sqrt(qmax);
			lam1 = q + sqrt(qmax +gam);

			/* SET UP DISSIPATIVE COEFFICIENTS */
			hp_ins_gbl->tau(tind,0) = adis*h/(jcb*sqrt(gam));
			hp_ins_gbl->tau(tind,NV-1) = qmax*hp_ins_gbl->tau(tind,0);

			/* SET UP DIAGONAL PRECONDITIONER */
			// jcb *= 8.*nu*(1./(hmax*hmax) +1./(h*h)) +2*lam1/h +2*sqrt(gam)/hmax +gbl->bd(0);
			jcb *= 2.*nu*(1./(hmax*hmax) +1./(h*h)) +3*lam1/h;  // heuristically tuned
			jcb *= RAD((pnts(v(0))(0) +pnts(v(1))(0) +pnts(v(2))(0))/3.);

			hp_gbl->tprcn_ut(tind,0,0) = hp_ins_gbl->rho*jcb;    
			hp_gbl->tprcn_ut(tind,1,1) = hp_ins_gbl->rho*jcb;      
			hp_gbl->tprcn_ut(tind,2,2) = jcb/gam;
			hp_gbl->tprcn_ut(tind,0,2) = jcb*ubar/gam;
			hp_gbl->tprcn_ut(tind,1,2) = jcb*vbar/gam;
			for(i=0;i<3;++i) {
				hp_gbl->vprcn_ut(v(i),Range::all(),Range::all())  += basis::tri(log2p)->vdiag()*hp_gbl->tprcn_ut(tind,Range::all(),Range::all());
				if (basis::tri(log2p)->sm() > 0) {
					side = tri(tind).seg(i);
					hp_gbl->sprcn_ut(side,Range::all(),Range::all()) += hp_gbl->tprcn_ut(tind,Range::all(),Range::all());
				}
			}
		}
	}
	return(tri_hp::setup_preconditioner() +err);
}
