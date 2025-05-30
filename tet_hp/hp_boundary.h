/*
 *  hp_boundary.h
 *  spectral_hp
 *
 *  Created by Brian Helenbrook on 9/3/05.
 *  Copyright 2005 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef _hp_boundary_h_
#define _hp_boundary_h_

#include "tet_hp.h"
#include <myblas.h>
#include "hp_boundary.h"

class hp_vrtx_bdry : public vgeometry_interface<3> {
	protected:
		std::string mytype;
		tet_hp& x;
		vrtx_bdry& base;

	public:
		init_bdry_cndtn *ibc;
		hp_vrtx_bdry(tet_hp& xin, vrtx_bdry &bin) : x(xin), base(bin) {mytype = "plain";ibc=x.hp_gbl->ibc;}
		hp_vrtx_bdry(const hp_vrtx_bdry &inbdry,tet_hp& xin, vrtx_bdry &bin) : x(xin), base(bin), mytype(inbdry.mytype) {}
		virtual hp_vrtx_bdry* create(tet_hp& xin, vrtx_bdry &bin) const {return new hp_vrtx_bdry(*this,xin,bin);}
		virtual void init(input_map& inmap) {} /**< This is to read definition data only (not solution data) */
		virtual void copy(const hp_vrtx_bdry& tgt) {}
		virtual ~hp_vrtx_bdry() {}
		void setvalues(init_bdry_cndtn *ibc, Array<int,1>& dirichlets, int ndirichlets);

		/* input output functions */
	/* input output functions */
	virtual void output(const std::string& filename, tet_hp::filetype typ,int tlvl = 0) {
		switch(typ) {
			case(tet_hp::text): {
				std::string fname;
				fname = filename +"_" +x.gbl->idprefix +".txt";
				ofstream fout;
				fout.open(fname.c_str(),std::ofstream::out | std::ofstream::app);
				fout << base.idprefix << " " << mytype << std::endl;
				fout.close();
				break;
			}
//			case(tet_hp::tecplot): {
//				if (report_flag) {
//					streamsize oldprecision = (*x.gbl->log).precision(10);
//					*x.gbl->log << base.idprefix << " position: " << x.pnts(base.pnt) << std::endl;
//					*x.gbl->log << base.idprefix << " value: " << x.ug.v(base.pnt,Range::all()) << std::endl;
//					(*x.gbl->log).precision(oldprecision);
//				}
//				break;
//			}
			default:
				break;
		}
		return;
	}
	
		/** This is to read solution data **/
		virtual void input(ifstream& fin,tet_hp::filetype typ,int tlvl = 0) {
			std::string idin,mytypein;

			switch(typ) {
				case(tet_hp::text):
				fin >> idin >> mytypein;
				break;
				default:
				break;
			}
			return;
		}

		/* BOUNDARY CONDITION FUNCTIONS */
		virtual void vdirichlet() {}
		virtual void vdirichlet2d() {} //!< SPECIAL CASE OF POINT BOUNDARY CONDITION FOR SURFACE FIELD
		virtual void vdirichlet3d() {} //!< SPECIAL CASE OF POINT BOUNDARY CONDITION FOR 3D FIELD
		virtual void pmatchsolution_snd(int phase, FLT *pdata, int vrtstride=1) {base.ploadbuff(boundary::all,pdata,0,x.NV-1,x.NV*vrtstride);}
		virtual void pmatchsolution_rcv(int phase, FLT *pdata, int vrtstride=1) {base.pfinalrcv(boundary::all_phased,phase,boundary::symmetric,boundary::average,pdata,0,x.NV-1,x.NV*vrtstride);}
			
		/* FOR COUPLED DYNAMIC BOUNDARIES */
        virtual int setup_preconditioner() {return(0);}
		virtual void tadvance() {
			int pnt = base.pnt;
			base.mvpttobdry(x.pnts(pnt));
		}
		virtual void calculate_unsteady_sources() {}
		virtual void rsdl(int stage) {}
		virtual void update(int stage) {}
		virtual void mg_restrict() {} 
		virtual void mg_prolongate() {} 
};


class hp_edge_bdry : public egeometry_interface<3> {
	public:
		std::string mytype;
		tet_hp& x;
		edge_bdry &base;
		const hp_edge_bdry *adapt_storage;
		init_bdry_cndtn *ibc;
		bool curved, coupled;
		Array<TinyVector<FLT,tet_mesh::ND>,2> crv;
		Array<Array<TinyVector<FLT,tet_mesh::ND>,2>,1> crvbd;
		Array<TinyMatrix<FLT,tet_mesh::ND,MXGP>,2> dxdt;

	public:
		hp_edge_bdry(tet_hp& xin, edge_bdry &bin) : x(xin), base(bin), curved(false), coupled(false) {mytype = "plain";ibc=x.hp_gbl->ibc;}
		hp_edge_bdry(const hp_edge_bdry &inbdry, tet_hp& xin, edge_bdry &bin) : mytype(inbdry.mytype), x(xin), base(bin), adapt_storage(inbdry.adapt_storage), ibc(inbdry.ibc), curved(inbdry.curved), coupled(inbdry.coupled) {
			if (curved && !x.coarse_level) {
				crv.resize(base.maxseg,x.em0);
				crvbd.resize(x.gbl->nhist+1);
				for(int i=1;i<x.gbl->nhist+1;++i)
					crvbd(i).resize(base.maxseg,x.em0);
				crvbd(0).reference(crv);
			}
			dxdt.resize(x.log2pmax+1,base.maxseg);
			base.resize_buffers(base.maxseg*(x.em0+2)*x.NV);   
		}
		virtual hp_edge_bdry* create(tet_hp& xin, edge_bdry &bin) const {return(new hp_edge_bdry(*this,xin,bin));}
		virtual void init(input_map& inmap);
		virtual void copy(const hp_edge_bdry& tgt);
		virtual ~hp_edge_bdry() {}
			
		/* input output functions */
		virtual void output(const std::string& filename, tet_hp::filetype typ,int tlvl);
		/** This is to read solution data **/
		virtual void input(ifstream& fin,tet_hp::filetype typ,int tlvl = 0); 
		void setvalues(init_bdry_cndtn *ibc, Array<int,1>& dirichlets, int ndirichlets);

		/* CURVATURE FUNCTIONS */
		bool is_curved() {return(curved);}
		FLT& crde(int ind, int mode, int dir) {return(crv(ind,mode)(dir));}
		FLT& crdebd(int tlvl, int ind, int mode, int dir) {return(crvbd(tlvl)(ind,mode)(dir));}
		void curv_init(int tlvl = 0);

		/* BOUNDARY CONDITION FUNCTIONS */
		virtual void maxres() {}
		virtual void vdirichlet() {}
		virtual void vdirichlet3d() {}  //!< SPECIAL CASE OF EDGE BOUNDARY CONDITION FOR 3D FIELD
		virtual void edirichlet() {}
		virtual void edirichlet3d() {}
		virtual void pmatchsolution_snd(int phase, FLT *vdata, int vrtstride=1) {base.ploadbuff(boundary::all,vdata,0,x.NV-1,x.NV*vrtstride);}
		virtual void pmatchsolution_rcv(int phase, FLT *vdata, int vrtstride=1) {base.pfinalrcv(boundary::all_phased,phase,boundary::symmetric,boundary::average,vdata,0,x.NV-1,x.NV*vrtstride);}
		virtual void smatchsolution_snd(int phase, FLT *sdata, int bgnmode, int endmode, int modestride) {
			base.sloadbuff(boundary::all,sdata,bgnmode*x.NV,(endmode+1)*x.NV-1,x.NV*modestride);
			return;
		}
		virtual void smatchsolution_rcv(int phase,FLT *sdata, int bgnmode, int endmode, int modestride) {
		base.sfinalrcv(boundary::all_phased,phase,boundary::symmetric,boundary::average,sdata,bgnmode*x.NV,(endmode+1)*x.NV-1,x.NV*modestride);
		}
		/* FOR COUPLED DYNAMIC BOUNDARIES */
        virtual int setup_preconditioner() {return(0);}
		virtual void tadvance();
		virtual void calculate_unsteady_sources();
		virtual void rsdl(int stage) {}
		virtual void update(int stage) {}
		virtual void mg_restrict() {} 
		virtual void mg_prolongate() {}  
		
		/* ADAPTATION FUNCTIONS */
//     virtual void updatevdata_bdry(int bel,int endpt,hp_edge_bdry *bin) {}
//     virtual void movevdata_bdry(int bel,int endpt,hp_edge_bdry *bin) {}
//     virtual void updatesdata_bdry(int bel,hp_edge_bdry *bin) {}
//     virtual void movesdata_bdry(int bel,hp_edge_bdry *tgt) {
//       int sind,tgtel,step,m,n;
//        
//       if (!curved || !x.em0) return;
//     
//       sind = base.seg(bel);
//       tgtel = tgt->x.getbdryel(tgt->x.ed(sind).tet(1));// may have screwed this up
//             
//       for(step=0;step<gbl->nadapt;++step) {
//         for(m=0;m<x.em0;++m) {
//            for(n=0;n<x.ND;++n) {
//              crdebd(step,bel,m,n) = tgt->crdebd(step,tgtel,m,n);
//            }
//         }
//       }
//       return;
//     }

		/* SEARCH FUNCTIONS */
		virtual void findandmovebdrypt(TinyVector<FLT,tet_mesh::ND>& xp,int &bel,FLT &psi) const;
		virtual void mvpttobdry(int nseg, FLT psi, TinyVector<FLT,tet_mesh::ND> &pt);
};


class hp_face_bdry : public fgeometry_interface<3> {
	public:
		std::string mytype;
		tet_hp& x;
		face_bdry &base;
		const hp_face_bdry *adapt_storage;
		init_bdry_cndtn *ibc;
		bool curved, coupled;
		Array<TinyVector<FLT,tet_mesh::ND>,2> ecrv;
		Array<Array<TinyVector<FLT,tet_mesh::ND>,2>,1> ecrvbd;
		Array<TinyVector<FLT,tet_mesh::ND>,2> fcrv;
		Array<Array<TinyVector<FLT,tet_mesh::ND>,2>,1> fcrvbd;
		Array<TinyVector<TinyVector<TinyVector<FLT,MXGP>,MXGP>,tet_mesh::ND>,2> dxdt;

	public:
		hp_face_bdry(tet_hp& xin, face_bdry &bin) : x(xin), base(bin), curved(false), coupled(false) {mytype = "plain";ibc=x.hp_gbl->ibc;}
		hp_face_bdry(const hp_face_bdry &inbdry, tet_hp& xin, face_bdry &bin) : mytype(inbdry.mytype), x(xin), base(bin), adapt_storage(inbdry.adapt_storage), ibc(inbdry.ibc), curved(inbdry.curved), coupled(inbdry.coupled) {
			if (curved && !x.coarse_level) {
				ecrvbd.resize(x.gbl->nhist+1);
				ecrv.resize(base.maxpst,x.em0);
				for(int i=1;i<x.gbl->nhist+1;++i)
					ecrvbd(i).resize(base.maxpst,x.em0);
				ecrvbd(0).reference(ecrv);
				
				fcrvbd.resize(x.gbl->nhist+1);
				fcrv.resize(base.maxpst,x.fm0);
				for(int i=1;i<x.gbl->nhist+1;++i)
					fcrvbd(i).resize(base.maxpst,x.fm0);
				fcrvbd(0).reference(fcrv);
			}
			
			dxdt.resize(x.log2pmax+1,base.maxpst);
			base.resize_buffers(base.maxpst*(x.fm0+2)*x.NV);	
		}
		virtual hp_face_bdry* create(tet_hp& xin, face_bdry &bin) const {return(new hp_face_bdry(*this,xin,bin));}
		virtual void init(input_map& inmap);
		virtual void copy(const hp_face_bdry& tgt);
		virtual ~hp_face_bdry() {}
			
		/* input output functions */
		virtual void output(const std::string& filename, tet_hp::filetype typ,int tlvl);
		/** This is to read solution data **/
		virtual void input(ifstream& fin,tet_hp::filetype typ,int tlvl = 0); 
		void setvalues(init_bdry_cndtn *ibc, Array<int,1>& dirichlets, int ndirichlets);

		/* CURVATURE FUNCTIONS */
		bool is_curved() {return(curved);}
		FLT& crde(int ind, int mode, int dir) {return(ecrv(ind,mode)(dir));}
		FLT& crdebd(int tlvl, int ind, int mode, int dir) {return(ecrvbd(tlvl)(ind,mode)(dir));}
		FLT& crdf(int ind, int mode, int dir) {return(fcrv(ind,mode)(dir));}
		FLT& crdfbd(int tlvl, int ind, int mode, int dir) {return(fcrvbd(tlvl)(ind,mode)(dir));}
		void curv_init(int tlvl = 0);

		/* BOUNDARY CONDITION FUNCTIONS */
		virtual void maxres() {}
		virtual void vdirichlet() {}
		virtual void edirichlet() {}
		virtual void fdirichlet() {}
		virtual void apply_sparse_dirichlet(bool compressed_column) {}
		virtual void element_rsdl(int find,int stage) {x.lf = 0.0;}

		virtual void pmatchsolution_snd(int phase, FLT *vdata, int vrtstride=1) {base.ploadbuff(boundary::all,vdata,0,x.NV-1,x.NV*vrtstride);}
		virtual void pmatchsolution_rcv(int phase, FLT *vdata, int vrtstride=1) {base.pfinalrcv(boundary::all_phased,phase,boundary::symmetric,boundary::average,vdata,0,x.NV-1,x.NV*vrtstride);}
		virtual void smatchsolution_snd(int phase, FLT *sdata, int bgnmode, int endmode, int modestride) {
			base.sloadbuff(boundary::all_phased,sdata,bgnmode*x.NV,(endmode+1)*x.NV-1,x.NV*modestride);
			return;
		}
		virtual void smatchsolution_rcv(int phase, FLT *sdata, int bgnmode, int endmode, int modestride) {
			base.sfinalrcv(boundary::all_phased,phase,boundary::symmetric,boundary::average,sdata,bgnmode*x.NV,(endmode+1)*x.NV-1,x.NV*modestride);
			return;
		}       
		virtual void tmatchsolution_snd(FLT *fdata, int bgnmode, int endmode, int modestride);
		virtual void tmatchsolution_rcv(FLT *fdata, int bgnmode, int endmode, int modestride); 
	
		/* FOR COUPLED DYNAMIC BOUNDARIES */
        virtual int setup_preconditioner() {return(0);}
		virtual void tadvance();
		virtual void calculate_unsteady_sources();
		virtual void rsdl(int stage) {}
		virtual void update(int stage) {}
		virtual void modify_boundary_residual() {}
		virtual void mg_restrict() {} 
		virtual void mg_prolongate() {}  
		
		/* ADAPTATION FUNCTIONS */
//     virtual void updatevdata_bdry(int bel,int endpt,hp_edge_bdry *bin) {}
//     virtual void movevdata_bdry(int bel,int endpt,hp_edge_bdry *bin) {}
//     virtual void updatesdata_bdry(int bel,hp_edge_bdry *bin) {}
//     virtual void movesdata_bdry(int bel,hp_edge_bdry *tgt) {
//       int sind,tgtel,step,m,n;
//       
//       if (!curved || !x.em0) return;
//   
//       sind = base.el(bel);
//       tgtel = tgt->x.getbdryel(tgt->x.ed(sind).tri(1));
//              
//       for(step=0;step<gbl->nadapt;++step) {
//         for(m=0;m<x.em0;++m) {
//            for(n=0;n<x.ND;++n) {
//              crdfbd(step,bel,m,n) = tgt->crdfbd(step,tgtel,m,n);
//            }
//         }
//       }
//       return;
//     }

		/* SEARCH FUNCTIONS */
       virtual void findandmovebdrypt(TinyVector<FLT,tet_mesh::ND>& xp,int &tind, FLT &r, FLT &s) const;
	   virtual void mvpttobdry(int tind, FLT r, FLT s, TinyVector<FLT,tet_mesh::ND> &pt);
	   virtual void mvpttobdry(int nseg, FLT psi, TinyVector<FLT,tet_mesh::ND> &pt);

};

#endif
