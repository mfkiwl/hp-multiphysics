/*
 *  ins_bdry.h
 *  spectral_hp
 *
 *  Created by Brian Helenbrook on 1/12/06.
 *  Copyright 2006 __MyCompanyName__. All rights reserved.
 *
 */

/*
 *  cd_bdry.h
 *  spectral_hp
 *
 *  Created by Brian Helenbrook on 11/7/05.
 *  Copyright 2005 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef _bdry_ins_h_
#define _bdry_ins_h_


#include "tet_hp_ins.h"
#include "../hp_boundary.h"
#include <myblas.h>
//#include <blitz/tinyvec-et.h>
#include <blitz/array.h>
#include <symbolic_function.h>

using namespace blitz;


//#define DETAILED_DT
//#define DETAILED_MINV

//#define L2_ERROR


namespace bdry_ins {

	class generic : public hp_face_bdry {
	protected:
		tet_hp_ins &x;
		bool report_flag;
#ifdef L2_ERROR
		symbolic_function<2> l2norm;
#endif
		enum bctypes {ess, nat, mix};
		
	public:
		Array<FLT,1> total_flux,diff_flux,conv_flux;
		FLT circumference,moment,convect,circulation;
		
	public:
		generic(tet_hp_ins &xin, face_bdry &bin) : hp_face_bdry(xin,bin), x(xin) {mytype = "generic";}
		generic(const generic& inbdry, tet_hp_ins &xin, face_bdry &bin) : hp_face_bdry(inbdry,xin,bin), x(xin), report_flag(inbdry.report_flag) {
			if (report_flag) {
#ifdef L2_ERROR
				l2norm = inbdry.l2norm;
#endif
				total_flux.resize(x.NV);
				diff_flux.resize(x.NV);
				conv_flux.resize(x.NV);  
			}
		}
		generic* create(tet_hp& xin, face_bdry &bin) const {return new generic(*this,dynamic_cast<tet_hp_ins&>(xin),bin);}
		void init(input_map& inmap) {
			hp_face_bdry::init(inmap);
			std::string keyword = base.idprefix +"_report";
			inmap.getwdefault(keyword,report_flag,false);
			
			if (report_flag) {
#ifdef L2_ERROR
				l2norm.init(input,base.idprefix+"_norm");
#endif
				total_flux.resize(x.NV);
				diff_flux.resize(x.NV);
				conv_flux.resize(x.NV);            
			}
		}
		void output(std::ostream& fout, tet_hp::filetype typ,int tlvl = 0);
	};
		

	class neumann : public generic {
		protected:
			virtual void flux(Array<FLT,1>& u, TinyVector<FLT,tet_mesh::ND> xpt, TinyVector<FLT,tet_mesh::ND> mv, TinyVector<FLT,tet_mesh::ND> norm, Array<FLT,1>& flx) {

				/* CONTINUITY */
				flx(x.NV-1) = x.hp_ins_gbl->rho*((u(0) -mv(0))*norm(0) +(u(1) -mv(1))*norm(1)+(u(2) -mv(2))*norm(2));

				/* X&Y MOMENTUM */
#ifdef INERTIALESS
				for (int n=0;n<tet_mesh::ND;++n)
						flx(n) = ibc->f(x.NV-1, xpt, x.gbl->time)*norm(n);
#else
				for (int n=0;n<tet_mesh::ND;++n)
						flx(n) = flx(x.NV-1)*u(n) +ibc->f(x.NV-1, xpt, x.gbl->time)*norm(n);
#endif

				/* EVERYTHING ELSE */
					for (int n=tet_mesh::ND;n<x.NV-1;++n)
						flx(n) = flx(x.NV-1)*u(n);

				return;
			}
		
		public:
			neumann(tet_hp_ins &xin, face_bdry &bin) : generic(xin,bin) {mytype = "neumann";}
			neumann(const neumann& inbdry, tet_hp_ins &xin, face_bdry &bin) : generic(inbdry,xin,bin) {}
			neumann* create(tet_hp& xin, face_bdry &bin) const {return new neumann(*this,dynamic_cast<tet_hp_ins&>(xin),bin);}
			void rsdl(int stage);
			void element_rsdl(int find,int stage);
	};



	class inflow : public neumann {  
	protected:
		Array<int,1> dirichlets;
		int ndirichlets;
		void flux(Array<FLT,1>& u, TinyVector<FLT,tet_mesh::ND> xpt, TinyVector<FLT,tet_mesh::ND> mv, TinyVector<FLT,tet_mesh::ND> norm,  Array<FLT,1>& flx) {
			
			/* CONTINUITY */
			flx(x.NV-1) = x.hp_ins_gbl->rho*((u(0) -mv(0))*norm(0) +(u(1) -mv(1))*norm(1)+(u(2) -mv(2))*norm(2));
			
			/* EVERYTHING ELSE DOESN'T MATTER */
			for (int n=0;n<x.NV-1;++n)
				flx(n) = 0.0;
			
			return;
		}
		
	public:
		inflow(tet_hp_ins &xin, face_bdry &bin) : neumann(xin,bin) {
			mytype = "inflow";
			ndirichlets = x.NV-1;
			dirichlets.resize(x.NV-1);
			for (int n=0;n<x.NV-1;++n)
				dirichlets(n) = n;
		}
        inflow(const inflow& inbdry, tet_hp_ins &xin, face_bdry &bin) : neumann(inbdry,xin,bin), ndirichlets(inbdry.ndirichlets) {dirichlets.resize(ndirichlets); dirichlets=inbdry.dirichlets;}
		inflow* create(tet_hp& xin, face_bdry &bin) const {return new inflow(*this,dynamic_cast<tet_hp_ins&>(xin),bin);}
		
		void vdirichlet() {
			int v0;
			
			for(int j=0;j<base.npnt;++j) {
				v0 = base.pnt(j).gindx;
				x.hp_gbl->res.v(v0,Range(0,x.NV-2)) = 0.0;
			}
		}
		
//		void edirichlet(int mode) {
//			int sind;
//			
//			for(int j=0;j<base.nseg;++j) {
//				sind = base.seg(j).gindx;
//				x.hp_gbl->res.e(sind,mode,Range(0,x.NV-2)) = 0.0;
//			}
//		}
		void edirichlet() {
			int sind;
			if (basis::tet(x.log2p).em > 0) {
				for(int j=0;j<base.nseg;++j) {
					sind = base.seg(j).gindx;
					x.hp_gbl->res.e(sind,Range(0,basis::tet(x.log2p).em-1),Range(0,x.NV-2)) = 0.0;
				}
			}
		}		
//		void fdirichlet(int mode) {
//			int find;
//			
//			for(int j=0;j<base.ntri;++j) {
//				find = base.tri(j).gindx;
//				x.hp_gbl->res.f(find,mode,Range(0,x.NV-2)) = 0.0;
//			}
//		}
		void fdirichlet() {
			int find;
			if (basis::tet(x.log2p).fm > 0) {
				for(int j=0;j<base.ntri;++j) {
					find = base.tri(j).gindx;
					x.hp_gbl->res.f(find,Range(0,basis::tet(x.log2p).fm-1),Range(0,x.NV-2)) = 0.0;
				}
			}
		}	
		
//		void apply_sparse_dirichlet(bool compressed_column) {
//			/* only works if pressure is 4th variable */
//			int gind;
//			int em=basis::tet(x.log2p).em;
//			int fm=basis::tet(x.log2p).fm;
//			
//			for(int i=0;i<base.npnt;++i){
//				gind = base.pnt(i).gindx*x.NV;
//				for(int n=0;n<x.NV-1;++n)
//					x.sparse_dirichlet(gind+n,compressed_column);
//			}
//			
//			for(int i=0;i<base.nseg;++i){
//				gind = x.npnt*x.NV+base.seg(i).gindx*em*x.NV;
//				for(int m=0; m<em; ++m)
//					for(int n=0;n<x.NV-1;++n)
//						x.sparse_dirichlet(gind+m*x.NV+n,compressed_column);
//			}
//			
//			for(int i=0;i<base.ntri;++i){
//				gind = x.npnt*x.NV+x.nseg*em*x.NV+base.tri(i).gindx*fm*x.NV;
//				for(int m=0; m<fm; ++m)
//					for(int n=0;n<x.NV-1;++n)
//						x.sparse_dirichlet(gind+m*x.NV+n,compressed_column);
//			}			
//		}
		
		void tadvance() {
			hp_face_bdry::tadvance();
			setvalues(ibc,dirichlets,ndirichlets);
		};
	};
	
	
	class symmetry : public generic {  
		protected:
			int dir;
		public:
			symmetry(tet_hp_ins &xin, face_bdry &bin) : generic(xin,bin) {mytype = "symmetry";}
			symmetry(const symmetry& inbdry, tet_hp_ins &xin, face_bdry &bin) : generic(inbdry,xin,bin), dir(inbdry.dir) {}
			symmetry* create(tet_hp& xin, face_bdry &bin) const {return new symmetry(*this,dynamic_cast<tet_hp_ins&>(xin),bin);}
			void init(input_map& inmap) {
				generic::init(inmap);
				std::string keyword = base.idprefix +"_dir";
				inmap.getwdefault(keyword,dir,0);
			}
		
			void vdirichlet() {
				int v0;
				
				for(int j=0;j<base.npnt;++j) {
					v0 = base.pnt(j).gindx;
					x.hp_gbl->res.v(v0,dir) = 0.0;
				}
			}
			
			void edirichlet() {
				int sind;
				if (basis::tet(x.log2p).em > 0) {
					for(int j=0;j<base.nseg;++j) {
						sind = base.seg(j).gindx;
						x.hp_gbl->res.e(sind,Range(0,basis::tet(x.log2p).em-1),dir) = 0.0;
					}
				}
			}		

			void fdirichlet() {
				int find;
				if (basis::tet(x.log2p).fm > 0) {
					for(int j=0;j<base.ntri;++j) {
						find = base.tri(j).gindx;
						x.hp_gbl->res.f(find,Range(0,basis::tet(x.log2p).fm-1),dir) = 0.0;
					}
				}
			}	
			
			//		void apply_sparse_dirichlet(bool compressed_column) {
			//			/* only works if pressure is 4th variable */
			//			int gind;
			//			int em=basis::tet(x.log2p).em;
			//			int fm=basis::tet(x.log2p).fm;
			//			
			//			for(int i=0;i<base.npnt;++i){
			//				gind = base.pnt(i).gindx*x.NV;
			//				for(int n=0;n<x.NV-1;++n)
			//					x.sparse_dirichlet(gind+n,compressed_column);
			//			}
			//			
			//			for(int i=0;i<base.nseg;++i){
			//				gind = x.npnt*x.NV+base.seg(i).gindx*em*x.NV;
			//				for(int m=0; m<em; ++m)
			//					for(int n=0;n<x.NV-1;++n)
			//						x.sparse_dirichlet(gind+m*x.NV+n,compressed_column);
			//			}
			//			
			//			for(int i=0;i<base.ntri;++i){
			//				gind = x.npnt*x.NV+x.nseg*em*x.NV+base.tri(i).gindx*fm*x.NV;
			//				for(int m=0; m<fm; ++m)
			//					for(int n=0;n<x.NV-1;++n)
			//						x.sparse_dirichlet(gind+m*x.NV+n,compressed_column);
			//			}			
			//		}
			
			void tadvance();
	};
	
	
	
	class applied_stress : public neumann {
		Array<symbolic_function<3>,1> stress;

		protected:
			void flux(Array<FLT,1>& u, TinyVector<FLT,tet_mesh::ND> xpt, TinyVector<FLT,tet_mesh::ND> mv, TinyVector<FLT,tet_mesh::ND> norm, Array<FLT,1>& flx) {

				/* CONTINUITY */
				flx(x.NV-1) = x.hp_ins_gbl->rho*((u(0) -mv(0))*norm(0) +(u(1) -mv(1))*norm(1)+(u(2) -mv(2))*norm(2));

				FLT length = sqrt(norm(0)*norm(0) +norm(1)*norm(1) +norm(2)*norm(2));
				/* XYZ MOMENTUM */

#ifdef INERTIALESS
				for (int n=0;n<tet_mesh::ND;++n)
					flx(n) = -stress(n).Eval(xpt,x.gbl->time)*length +ibc->f(x.NV-1, xpt, x.gbl->time)*norm(n);
#else
				for (int n=0;n<tet_mesh::ND;++n)
					flx(n) = flx(x.NV-1)*u(n) -stress(n).Eval(xpt,x.gbl->time)*length +ibc->f(x.NV-1, xpt, x.gbl->time)*norm(n);
#endif

				/* EVERYTHING ELSE */
				for (int n=tet_mesh::ND;n<x.NV-1;++n)
					flx(n) = flx(x.NV-1)*u(n);

				return;
			}
		public:
			applied_stress(tet_hp_ins &xin, face_bdry &bin) : neumann(xin,bin) {mytype = "applied_stress";}
			applied_stress(const applied_stress& inbdry, tet_hp_ins &xin, face_bdry &bin) : neumann(inbdry,xin,bin), stress(inbdry.stress) {}
			applied_stress* create(tet_hp& xin, face_bdry &bin) const {return new applied_stress(*this,dynamic_cast<tet_hp_ins&>(xin),bin);}
			void init(input_map& inmap);
	};
	
}
#endif
