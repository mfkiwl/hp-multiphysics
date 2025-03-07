/*
 *  tri_hp.h
 *  planar++
 *
 *  Created by helenbrk on Mon Oct 01 2001.
 *  Copyright (c) 2001 __CompanyName__. All rights reserved.
 *
 */

#ifndef _tri_hp_h_
#define _tri_hp_h_

#include <r_tri_mesh.h>
#include <float.h>
#include <tri_basis.h>
#include <blocks.h>
#include <myblas.h>

#ifdef petsc
#include <petscksp.h>
#endif

#define DIRK

#ifdef AXISYMMETRIC
#define RAD(r) (r)
#else
#define RAD(r) 1
#endif

#define MAXP 4
#define MXGP MAXP+2
#define MXTM (MAXP+1)*(MAXP+2)/2 

// #define MESH_REF_VEL
// #define ALLCURVED

class hp_vrtx_bdry;
class hp_edge_bdry;

class init_bdry_cndtn {
public:
    virtual FLT f(int n, TinyVector<FLT,tri_mesh::ND> x, FLT time) = 0;
    virtual void init(input_map &inmap, std::string idnty) {};
    virtual ~init_bdry_cndtn() {};
};

class tri_hp_helper;

/** This class is just the data storage and nothing for multigrid */
class tri_hp : public r_tri_mesh  {
public:
    int NV; /**> Number of vector variables */
    int p0, sm0, im0;  /**> Initialization values */
    int log2p; /**> index of basis to use in global basis::tri array */
    int log2pmax; /**> Initialization value of log2p */
    enum movementtype {fixed,uncoupled_rigid,coupled_rigid,uncoupled_deformable,coupled_deformable} mmovement;
    
    /* WORK ARRAYS */
    Array<TinyMatrix<FLT,MXGP,MXGP>,1> u,res;
    Array<TinyMatrix<FLT,MXGP,MXGP>,2> du;
    TinyVector<TinyMatrix<FLT,MXGP,MXGP>,ND> crd;
    TinyMatrix<FLT,MXGP,MXGP> cjcb;
    TinyMatrix<TinyMatrix<FLT,MXGP,MXGP>,ND,ND> dcrd;
    Array<TinyVector<FLT,MXTM>,1> uht,lf;
    TinyMatrix<FLT,ND,MXTM> cht, cf;
    Array<TinyMatrix<FLT,MXGP,MXGP>,2> bdwk;
    
    /** Stores vertex, side and interior coefficients of solution */
    struct vsi {
        Array<FLT,2> v; /**> vertex coefficients (npnt,NV) */
        Array<FLT,3> s; /**> side coefficient (nseg, NV) */
        Array<FLT,3> i; /**> interior coefficients (ntri,NV) */
    } ug;
    
#ifdef ALLCURVED
    bool allcurved; /**> flag to determine if all edges are curved or only boundary edges */
    struct vsi crv;  /**> array to hold mesh curvatures */
    Array<vsi,1> crvbd; /** array to hold backwards difference curvatures */
    /* Pointers to functions so that they can be overwritten in the input file */
    void (*pcrdtocht)(tri_hp *tp, int tind);
    void (*pcrdtocht_nhist)(tri_hp *tp, int tind,int nhist);
    void (*pcrdtocht1d)(tri_hp *tp, int sind);
    void (*pcrdtocht1d_nhist)(tri_hp *tp, int sind,int nhist);
#endif
    
    /** vertex boundary information */
    Array<hp_vrtx_bdry *,1> hp_vbdry;
    virtual hp_vrtx_bdry* getnewvrtxobject(int bnum, std::string name);
    /** edge boundary information */
    Array<hp_edge_bdry *,1> hp_ebdry;
    virtual hp_edge_bdry* getnewedgeobject(int bnum, std::string name);
    /** object to perform rigid mesh movement */
    tri_hp_helper *helper;
    
    /** Array for time history information */
    Array<vsi,1> ugbd;
    Array<Array<TinyVector<FLT,ND>,1>,1> vrtxbd; //!< Highest level contains pre-summed unsteady mesh velocity source
    Array<Array<FLT,4>,1> dugdt; //!< Precalculated unsteady sources at Gauss points
    Array<Array<FLT,4>,1> dxdt; //!< Precalculated mesh velocity sources at Gauss points
    
    /* Multigrid stuff needed on each mesh */
    bool isfrst; // FLAG TO SET ON FIRST ENTRY TO COARSE MESH
    bool coarse_flag;   // Flag to indicate coarse level
    Array<vsi,1> dres; //!< Driving term for multigrid
    Array<FLT,2> vug_frst; //!< Solution on first entry to coarse mesh
    FLT fadd; //!< Controls addition of residuals on coarse mesh
    
    /* THESE THINGS ARE SHARED BY MESHES OF THE SAME BLOCK */
    struct hp_global {
        
        /**< Pointer to adaptation solution storage
         * Also used for backwards difference storage in tadvance
         * could be used for ug0 res and res_r as well?
         */
        tri_hp *pstr;
        FLT curvature_sensitivity; /**<  sensitivity to boundary curvature  */
        TinyVector<FLT,3> eanda, eanda_recv; /**< Storage for calculation of error energy and area for adaptation */
        enum error_estimator_type {none,energy_norm,scale_independent};
        error_estimator_type error_estimator;
        
        /* SOLUTION STORAGE ON FIRST ENTRY TO NSTAGE */
        vsi ug0;
        
        /** Residual storage for equations */
        Array<FLT,1> res1d;
        vsi res;
        
        /* REAL PART FOR RESIDUAL STORAGE */
        Array<FLT,1> res_r1d;
        vsi res_r;
        
        /* RESIDUAL STORAGE FOR ENTRY TO MULTIGRID */
        Array<FLT,1> res0_1d;
        vsi res0;
        
        /* PRECONDITIONER  */
        bool diagonal_preconditioner;
        Array<FLT,2> vprcn, sprcn, tprcn;  // Diagonal preconditioner
        Array<FLT,3> vprcn_ut, sprcn_ut, tprcn_ut; // Lower triangle preconditioner
        
        /* INITIALIZATION AND BOUNDARY CONDITION FUNCTION */
        init_bdry_cndtn *ibc;
#ifdef MESH_REF_VEL
        TinyVector<FLT,ND> mesh_ref_vel;
#endif
        /* Time step factor for different polynomial degree */
        TinyVector<FLT,MXGP> cfl;
        
    };
    shared_ptr<hp_global> hp_gbl;
    
    virtual init_bdry_cndtn* getnewibc(std::string name);
    virtual tri_hp_helper* getnewhelper(std::string name);
    
    /* FUNCTIONS FOR MOVING GLOBAL TO LOCAL */
    void ugtouht(int tind); /**< Gathers globl u vector and stores it in uht */
    void ugtouht(int tind,int nhist); /**< Gathers for previous time steps */
    void ugtouht_bdry(int tind); /**< Gathers only boundary modes */
    void ugtouht_bdry(int tind, int nhist);
    void ugtouht1d(int sind);
    void ugtouht1d(int sind, int nhist);
    void crdtocht(int tind); /**< Gathers global coordinate vector and stores it in cht */
    void crdtocht(int tind, int nhist);
    void crdtocht1d(int sind);
    void crdtocht1d(int sind, int nhist);
    void restouht_bdry(int tind); // USED IN MINVRT
    
    /* THIS FUNCTION ADDS LF TO GLOBAL VECTORS */
    void lftog(int tind, vsi gvect); /**< gather local to global vector */
    
    /* SETUP V/S/T INFO */
    void setinfo();
    
public:
    tri_hp() : r_tri_mesh() {}
    virtual tri_hp* create() {return new tri_hp;}
    /* Fixme: Replace init with a constructor that accepts an input_map? */
    void init(input_map& inmap, shared_ptr<block_global> gin);
    void init(const multigrid_interface& in, init_purpose why=duplicate, FLT sizereduce1d=1.0);
    void tobasis(init_bdry_cndtn *ibc, int tlvl = 0);
    //void curvinit();
    
    /* Input / Output functions */
    enum filetype {tecplot, text, binary, adapt_diagnostic, auxiliary, vtk, vtu, netcdf, gmsh};
    TinyVector<filetype,3> output_type;
    filetype reload_type;
    void input(const std::string &name);
    void input(const std::string &name, filetype type, int tlvl = 0);
    void input(int size,Array<FLT,1> list, std::string filename,filetype typ); // inputs 1D FLT array
#ifdef ALLCURVED
    void load_curvatures(const std::string& filename, filetype typ, vsi crv); /** loads mesh curvature information */
#endif

    /** Outputs solution in various filetypes */
    void output(const std::string &name, block::output_purpose why);
    void output(const std::string &name, filetype type = tecplot, int tlvl = 0);
    void output(int size,Array<FLT,1> list, std::string filename, filetype type);  // outputs 1D FLT array
    
    /** Shift to next implicit time step */
    void tadvance();
    virtual void calculate_unsteady_sources();
    void reset_timestep();
    
    /** Makes sure vertex positions on boundaries coinside */
    void matchboundaries();
    
    /** Setup preconditioner */
    int setup_preconditioner();
    
    /** Calculate residuals */
    void rsdl() {rsdl(gbl->nstage);}
    virtual void rsdl(int stage);
    virtual void element_rsdl(int tind, int stage, Array<TinyVector<FLT,MXTM>,1> &uhat,Array<TinyVector<FLT,MXTM>,1> &lf_re,Array<TinyVector<FLT,MXTM>,1> &lf_im) {
        *gbl->log << "I shouldn't be in generic element_rsdl" << std::endl;
    }
    void jacobian() {} // Not filled this in yet
    virtual void element_jacobian(int tind, Array<FLT,2>&);
    
    /** Relax solution */
    void update();
    virtual void minvrt();
    void minvrt_test();
    
    /** Multigrid cycle */
    void mg_setup_preconditioner();
    void mg_preconditioner_snd();
    void mg_preconditioner_rcv();
    void mg_restrict();
    void mg_prolongate();
    void mg_source();
    
    /** Print errors */
    FLT max_residual;
    FLT maxres();
    
    /* FUNCTIONS FOR ADAPTION */
    void length();
    virtual void error_estimator() {};
    void adapt();
    void refineby2();
    void copy(const tri_hp &tgt);
    void append_halos();
    void transfer_halo_solutions();
    void movepdata(int frm, int to);
    void movepdata_bdry(int bnum,int bel,int endpt);
    void updatepdata(int v);
    void updatepdata_bdry(int bnum,int bel,int endpt);
    void movesdata(int frm, int to);
    void movesdata_bdry(int bnum,int bel);
    void updatesdata(int s);
    void updatesdata_bdry(int bnum,int bel);
    void movetdata(int frm, int to);
    void updatetdata(int t);
    bool findinteriorpt(TinyVector<FLT,2> pt, int &tind, FLT &r, FLT &s);
    void findandmvptincurved(TinyVector<FLT,2>& pt,int &tind, FLT &r, FLT &s);
    bool ptprobe(TinyVector<FLT,ND> xp, Array<FLT,1> uout, int& tind, int tlvl = 0);
    void ptprobe_bdry(int bnum, TinyVector<FLT,ND> xp, Array<FLT,1> uout, int tlvl);
    
    /* MESSAGE PASSING ROUTINES SPECIALIZED FOR SOLUTION CONTINUITY */
    void vc0load(int phase, FLT *pdata, int vrtstride=1);
    int vc0wait_rcv(int phase,FLT *pdata, int vrtsride=1);
    int vc0rcv(int phase,FLT *pdata, int vrtstride=1);
    void sc0load(FLT *sdata, int bgnmode, int endmode, int modestride);
    int sc0wait_rcv(FLT *sdata, int bgnmode, int endmode, int modestride);
    int sc0rcv(FLT *sdata, int bgnmode, int endmode, int modestride);
    
    /* Some other utilities */
    void partition(multigrid_interface& xmesh, int npart, int maxenum = 0, int maxvnum = 0); /**< Creates a partition from xmesh */
    virtual void l2error(init_bdry_cndtn *toCompare);
    void findmax(int bnum, FLT (*fxy)(TinyVector<FLT,ND> &x));
    void findintercept(int bnum, FLT (*fxy)(TinyVector<FLT,ND> &x));
    void integrated_averages(Array<FLT,1> a);
    void local_index_to_mesh_descriptor(int col, std::string& desc);
    
#ifdef petsc
    /* Sparse stuff */
    FLT under_relax;
    void petsc_jacobian();
    void petsc_premultiply_jacobian();
    void test_jacobian();
    void enforce_mesh_continuity(Array<TinyVector<FLT,ND>,1>& pnts);
    void enforce_solution_continuity(vsi& ug);
    void petsc_rsdl();
    void petsc_update();
    void petsc_setup_preconditioner();
    
    void petsc_initialize();
    void petsc_finalize();
    void r_jacobian_dirichlet(Array<int,1> points, int dstart, int dstop);  // Interface to Jacobian for r_tri_mesh (for now)
    void petsc_to_ug();
    void ug_to_petsc();
    void petsc_make_1D_rsdl_vector(Array<FLT,1>);
    void petsc_output_vector(Vec petsc_vec);
    
    int jacobian_start;
    int jacobian_size;
    Mat  petsc_J;           /* Jacobian matrix */
    Vec  petsc_u,petsc_f,petsc_du;  /* solution,residual */
    KSP  ksp;               /* linear solver context */
    PC   pc;                 /* preconditioner */
    
#ifdef MY_SPARSE
    sparse_row_major J; // This block's Jacobian
    sparse_row_major J_mpi; // Coupling to other blocks
#endif
#endif	
    
    virtual ~tri_hp();
};

#ifdef ALLCURVED
void crdtocht_standard(tri_hp *tp, int tind); /**< Gathers global coordinate vector and stores it in cht */
void crdtocht_nhist_standard(tri_hp *tp, int tind, int nhist);
void crdtocht1d_standard(tri_hp *tp, int sind);
void crdtocht1d_nhist_standard(tri_hp *tp, int sind, int nhist);
void crdtocht_allcurved(tri_hp *tp, int tind); /**< Gathers global coordinate vector and stores it in cht */
void crdtocht_nhist_allcurved(tri_hp *tp, int tind, int nhist);
void crdtocht1d_allcurved(tri_hp *tp, int sind);
void crdtocht1d_nhist_allcurved(tri_hp *tp, int sind, int nhist);
#endif

/* THIS CLASS IS TO ALLOW SPECIAL THINGS LIKE RIGIDLY MOVING MESHES OR PARAMETER CHANGING ETC.. */
class tri_hp_helper {
protected:
    tri_hp &x;
    bool post_process;
    
public:
    tri_hp_helper(tri_hp& xin) : x(xin), post_process(false) {}
    tri_hp_helper(const tri_hp_helper &in_help, tri_hp& xin) : x(xin), post_process(in_help.post_process) {}
    virtual tri_hp_helper* create(tri_hp& xin) { return new tri_hp_helper(*this,xin); }
    virtual ~tri_hp_helper() {};
    virtual void init(input_map& inmap, std::string idnty) {
        if (!inmap.get(idnty+"_post_process",post_process)) {
            inmap.getwdefault("post_process",post_process,false);
        }
    }
    virtual void tadvance() {
        if (post_process && x.gbl->substep == 0) {
            std::ostringstream nstr;
            std::string fname;
            nstr << x.gbl->tstep << std::flush;
            fname = "rstrt" +nstr.str();
            if (!x.coarse_flag) x.input(fname);
        }
    }
    virtual int setup_preconditioner() {return(0);}
    virtual void rsdl(int stage) {}
    virtual void update(int stage) {}
    virtual void mg_restrict() {}
    virtual void output() {};
    
    /* Stuff to make jacobians for coupled problems */
    virtual int dofs(int dofs) { return 0;}
    virtual void non_sparse(Array<int,1> &nnzero,Array<int,1> &nnzero_mpi) {}
    virtual void jacobian() {};
    virtual void jacobian_dirichlet() {};
};

namespace basis {
#ifdef AXISYMMETRIC
extern tri_basis_array<1> tri;
#else
extern tri_basis_array<0> tri;
#endif
}

#endif
