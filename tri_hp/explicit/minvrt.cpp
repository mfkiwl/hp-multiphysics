#include "tri_hp_explicit.h"
#include "../hp_boundary.h"
#include <myblas.h>

/************************************************/
/**********        INVERT MASS MATRIX     **********/
/************************************************/
void tri_hp_explicit::minvrt() {
	int i,j,k,m,n,tind,sind,v0,indx,indx1,indx2,sgn,msgn;
	TinyVector<int,3> sign,side;
	Array<FLT,2> tinv(NV,NV);
	Array<FLT,1> temp(NV);
	int last_phase, mp_phase;
	const int lbm = basis::tri(log2p)->bm();
	const int lsm = basis::tri(log2p)->sm();

	/* LOOP THROUGH SIDES */
	if (basis::tri(log2p)->sm() > 0) {
		indx = 0;
		for(sind = 0; sind<nseg;++sind) {
			/* SUBTRACT SIDE CONTRIBUTIONS TO VERTICES */            
			for (k=0; k <basis::tri(log2p)->sm(); ++k) {
				for (i=0; i<2; ++i) {
					v0 = seg(sind).pnt(i);
					for(n=0;n<NV;++n)
						hp_gbl->res.v(v0,n) -= basis::tri(log2p)->sfmv(i,k)*hp_gbl->res.s(sind,k,n);
				}
				++indx;
			}
		}

		if (basis::tri(log2p)->im() > 0) {
			/* SUBTRACT INTERIORS */
			indx = 0;
			for(tind = 0; tind<ntri;++tind) {
				indx2 = 3;
				for (i=0; i<3; ++i) {
					v0 = tri(tind).pnt(i);
					for (k=0;k<basis::tri(log2p)->im();++k)
						for(n=0;n<NV;++n)
							hp_gbl->res.v(v0,n) -= basis::tri(log2p)->ifmb(i,k)*hp_gbl->res.i(tind,k,n);

					sind = tri(tind).seg(i);
					sgn = tri(tind).sgn(i);
					msgn = 1;
					for (j=0;j<basis::tri(log2p)->sm();++j) {
						for (k=0;k<basis::tri(log2p)->im();++k)
							for(n=0;n<NV;++n)
								hp_gbl->res.s(sind,j,n) -= msgn*basis::tri(log2p)->ifmb(indx2,k)*hp_gbl->res.i(tind,k,n);
						msgn *= sgn;
						++indx2;
					}
				}
				indx += basis::tri(log2p)->im();
			}
		}
	}

	hp_gbl->res.v(Range(0,npnt-1),Range::all()) *= hp_gbl->vprcn(Range(0,npnt-1),Range::all());

	for(last_phase = false, mp_phase = 0; !last_phase; ++mp_phase) {
		vc0load(mp_phase,hp_gbl->res.v.data());
		pmsgpass(boundary::all_phased,mp_phase,boundary::symmetric);
		last_phase = true;
		last_phase &= vc0wait_rcv(mp_phase,hp_gbl->res.v.data());
	}

	/* APPLY VERTEX DIRICHLET B.C.'S */
	for(i=0;i<nebd;++i)
		hp_ebdry(i)->vdirichlet();

	for(i=0;i<nvbd;++i)
		hp_vbdry(i)->vdirichlet();

	if(basis::tri(log2p)->sm() == 0) return;

	/* REMOVE VERTEX CONTRIBUTION FROM SIDE MODES */
	/* SOLVE FOR SIDE MODES */
	/* PART 1 REMOVE VERTEX CONTRIBUTIONS */
	for(tind=0;tind<ntri;++tind) {
		for(i=0;i<3;++i) {
			v0 = tri(tind).pnt(i);
			for(n=0;n<NV;++n)
				uht(n)(i) = hp_gbl->res.v(v0,n);
		}

		if (tri(tind).info > -1) {
			for(j=0;j<3;++j) {
				indx1 = 3;
				for(i=0;i<3;++i) {
					sind = tri(tind).seg(i);
					sgn  = tri(tind).sgn(i);
					msgn = 1;
					for(k=0;k<basis::tri(log2p)->sm();++k) {
					for(n=0;n<NV;++n)
						hp_gbl->res.s(sind,k,n) -= msgn*hp_explicit_gbl->mass[tind](indx1,j)*uht(n)(j);
					msgn *= sgn;
					++indx1;
					}
				}
			}
		}
		else {
			for(i=0;i<3;++i) {
				sind = tri(tind).seg(i);
				sgn  = tri(tind).sgn(i);
				for(j=0;j<3;++j) {
					indx1 = (i+j)%3;
					msgn = 1;
					for(k=0;k<basis::tri(log2p)->sm();++k) {
					for(n=0;n<NV;++n)
						hp_gbl->res.s(sind,k,n) -= msgn*basis::tri(log2p)->vfms(j,k)*uht(n)(indx1)*hp_gbl->tprcn(tind,n);
					msgn *= sgn;
					}
				}
			}
		}
	}


	for (int mode = 0; mode < basis::tri(log2p)->sm()-1; ++ mode) {
		/* SOLVE FOR SIDE MODE */
		if (hp_gbl->diagonal_preconditioner) {
			hp_gbl->res.s(Range(0,nseg-1),mode,Range::all()) *= hp_explicit_gbl->sprcn2(Range(0,nseg-1),mode,Range::all());
		}
		else {
			for(sind = 0; sind < nseg; ++sind) {
				DGETUS(&hp_gbl->sprcn_ut(sind,0,0), NV, NV, &hp_gbl->res.s(sind,mode,0));
				hp_gbl->res.s(sind,mode,Range::all()) /= basis::tri(log2p)->sdiag(mode);
			}
		}

		sc0load(hp_gbl->res.s.data(),mode,mode,hp_gbl->res.s.extent(secondDim));
		smsgpass(boundary::all,0,boundary::symmetric);
		sc0wait_rcv(hp_gbl->res.s.data(),mode,mode,hp_gbl->res.s.extent(secondDim));


		/* APPLY DIRCHLET B.C.S TO MODE */
		for(i=0;i<nebd;++i)
			hp_ebdry(i)->sdirichlet(mode);

		/* REMOVE MODE FROM HIGHER MODES */
		for(tind=0;tind<ntri;++tind) {
			if (tri(tind).info > -1) {

				/* LOAD MODES FOR GIVEN TRIANGLE */
				for(i=0;i<3;++i) {
					side(i) = tri(tind).seg(i);
					sign(i) = tri(tind).sgn(i);
					sgn      = (mode % 2 ? sign(i) : 1);
					for(n=0;n<NV;++n)
					uht(n)(i) = sgn*hp_gbl->res.s(side(i),mode,n);
				}


				///* REMOVE MODES J,K FROM MODE I,M */
				for(i=0;i<3;++i) {
					msgn = ( (mode +1) % 2 ? sign(i) : 1);
					for(m=mode+1;m<basis::tri(log2p)->sm();++m) {
						for(j=0;j<3;++j) {
//								if (i == j) continue;  // FOR 0 LOWER TRIANGULAR SIDE TO ITSELF
							for(n=0;n<NV;++n) {
								hp_gbl->res.s(side(i),m,n) -= msgn*hp_explicit_gbl->mass[tind](i*lsm+3+m,j*lsm+3+mode)*uht(n)(j);
							}
						}
						msgn *= sign(i);
					}
				}				
			}
			else {
				for(i=0;i<3;++i) {
					side(i) = tri(tind).seg(i);
					sign(i) = tri(tind).sgn(i);
					sgn      = (mode % 2 ? sign(i) : 1);
					for(n=0;n<NV;++n)
					uht(n)(i) = sgn*hp_gbl->res.s(side(i),mode,n)*hp_gbl->tprcn(tind,n);
				}

				/* REMOVE MODES J,K FROM MODE I,M */
				for(i=0;i<3;++i) {
					msgn = ( (mode +1) % 2 ? sign(i) : 1);
					for(m=mode+1;m<basis::tri(log2p)->sm();++m) {
					for(j=0;j<3;++j) {
						indx = (i+j)%3;
						for(n=0;n<NV;++n) {
							hp_gbl->res.s(side(i),m,n) -= msgn*basis::tri(log2p)->sfms(mode,m,j)*uht(n)(indx);
						}
					}
					msgn *= sign(i);
					}
				}
			}
		}
	}
	/* SOLVE FOR HIGHEST MODE */
	int mode = basis::tri(log2p)->sm()-1;
	if (hp_gbl->diagonal_preconditioner) {
		hp_gbl->res.s(Range(0,nseg-1),mode,Range::all()) *= hp_explicit_gbl->sprcn2(Range(0,nseg-1),mode,Range::all());
	}
	else {
		for(sind = 0; sind < nseg; ++sind) {
			DGETUS(&hp_gbl->sprcn_ut(sind,0,0), NV, NV, &hp_gbl->res.s(sind,mode,0));
			hp_gbl->res.s(sind,mode,Range::all()) /= basis::tri(log2p)->sdiag(mode);
		}
	}

	sc0load(hp_gbl->res.s.data(),mode,mode,hp_gbl->res.s.extent(secondDim));
	smsgpass(boundary::all,0,boundary::symmetric);
	sc0wait_rcv(hp_gbl->res.s.data(),mode,mode,hp_gbl->res.s.extent(secondDim));

	/* APPLY DIRCHLET B.C.S TO MODE */
	for(i=0;i<nebd;++i)
		hp_ebdry(i)->sdirichlet(mode);


	if (basis::tri(log2p)->im() == 0) return;

	/* SOLVE FOR INTERIOR MODES */
	for(tind = 0; tind < ntri; ++tind) {
		restouht_bdry(tind);
		if (tri(tind).info > -1) {

			for(k=0;k<basis::tri(log2p)->im();++k) {
				for (i=0;i<basis::tri(log2p)->bm();++i) {
					for (n=0;n<NV;++n)
					hp_gbl->res.i(tind,k,n) -= hp_explicit_gbl->mass[tind](k+lbm,i)*uht(n)(i);
				}
//					hp_gbl->res.i(tind,k,Range::all()) /= hp_explicit_gbl->mass[tind](k+lbm,k+lbm);  // DIAGONAL INTERIOR MASS MATRIX
			}



			/* THIS WAY FOR LOWER TRIANGULAR INTERIOR-INTERIOR MASS MATRIX */
			for(m=0;m<lsm-1;++m) {
				/* SOLVE FOR INTERIOR MODES AT THIS DEGREE */
				for (k=0;k<m+1;++k) {
					indx = (2*(lsm-1)-(k-1))*k/2 +(m-k);
					hp_gbl->res.i(tind,indx,Range::all()) /= hp_explicit_gbl->mass[tind](indx+lbm,indx+lbm);

					/* SUBTRACT FROM MODES AT HIGHER DEGREES */
					for (int m2=m+1;m2<lsm-1;++m2) {
					for (int k2=0;k2<m2+1;++k2) {
						indx2 = (2*(lsm-1)-(k2-1))*k2/2 +(m2-k2);
						hp_gbl->res.i(tind,indx2,Range::all()) -= hp_explicit_gbl->mass[tind](indx2+lbm,indx+lbm)*hp_gbl->res.i(tind,indx,Range::all());
					}
					}
				}
			}
		}
		else {
			DPBTRSNU2((double *) &basis::tri(log2p)->idiag(0,0),basis::tri(log2p)->ibwth()+1,basis::tri(log2p)->im(),basis::tri(log2p)->ibwth(),&(hp_gbl->res.i(tind,0,0)),NV);
			for(k=0;k<basis::tri(log2p)->im();++k) {
				hp_gbl->res.i(tind,k,Range::all()) /= hp_gbl->tprcn(tind,Range::all());

				for (i=0;i<basis::tri(log2p)->bm();++i)
					for(n=0;n<NV;++n) 
					hp_gbl->res.i(tind,k,n) -= basis::tri(log2p)->bfmi(i,k)*uht(n)(i);
			}
		}
	}

	return;
}


