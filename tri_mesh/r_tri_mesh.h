#ifndef _r_tri_mesh_h_
#define _r_tri_mesh_h_

#include "tri_mesh.h"
#include "block.h"
#include <map>
#include <string>
#include <sstream>
#include <iostream>


/* AN R-DEFORMABLE MULTI-GRID MESH OBJECT */
/* GOOD COMBINATIONS ARE: FIXX & FIX2X, FIXX & !FIX2X, AND !FIXX and !FIX2X */
/* DON'T DO !FIXX and FIX2X */

#define NO_FOURTH
#define GEOMETRIC

class r_side_bdry;
class r_vrtx_bdry;

class r_tri_mesh : public tri_mesh {
		protected:
			FLT r_cfl;
			FLT fadd;
			tri_mesh::filetype output_type;

			/* MESH VARIABLES */
			Array<FLT,1> ksprg;
			Array<FLT,1> kvol;
			Array<TinyVector<FLT,ND>,1> src;
			Array<TinyVector<FLT,ND>,1> pnts_frst;

	public:
            shared_ptr<block_global> gbl;

			Array<r_side_bdry *,1> r_sbdry;
			r_side_bdry* getnewedgeobject(int bnum, input_map& bdrydata);

			Array<r_vrtx_bdry *,1> r_vbdry;
			r_vrtx_bdry* getnewvrtxobject(int bnum, input_map& bdrydata);

	protected:
			/* SETUP SPRING CONSTANTS */
			/* LAPLACE CONSTANTS */
			void rklaplace();
#ifdef FOURTH
			/* NEEDED FOR BIHARMONIC METHOD */
			void calc_kvol();
#endif
			/* SPRING METHOD */
			void rksprg();

			/* CALCULATE COARSE SPRING CONSTANTS */
			/* USING INTERPOLATION OPERATORS */
			/* SHOULD WORK??? FOR BIHARMONIC, LAPLACIAN, & SPRING */
			void rkmgrid();

			/* CALCLATE SOURCE TERM */
			void zero_source();
			void sumsrc();

			void moveboundaries();

		public:
			/* POINTER TO THINGS SHARED IN BLOCK CONTAINER */
			/* CAN BE SHARED BETWEEN MGLEVELS BUT NOT DIFFERENT BLOCKS */
			/* NEED TO BE PUBLIC SO THEY CAN BE MANIPULATED BY B.C.'s */
			struct r_global {
				Array<FLT,1> diag;
				Array<TinyVector<FLT,2>,1> res;
				Array<TinyVector<FLT,2>,1> res1;
            };
            shared_ptr<r_global> r_gbl;
			~r_tri_mesh();

			/* ACCESSOR FUNCTIONS FOR COMPATIBILITY WITH MGBLOCK */
			void init(input_map& input, shared_ptr<block_global> gbl_in);
			void init(const multigrid_interface& in, init_purpose why=duplicate, FLT sizereduce1d=1.0);
			void output(const std::string &outname,block::output_purpose why) {tri_mesh::output(outname,output_type);}
			void mg_restrict();
			void mg_prolongate();
			void tadvance();
			void rsdl();
			void element_jacobian(int tind,Array<FLT,2> K); 
			void update();
			int setup_preconditioner();
			FLT maxres();
		private:
			bool isfrst;
};
#endif


