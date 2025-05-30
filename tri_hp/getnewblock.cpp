/*
 *  initfunc.cpp
 *  planar++
 *
 *  Created by helenbrk on Wed Oct 24 2001.
 *  Copyright (c) 2001 __MyCompanyName__. All rights reserved.
 *
 */

#include <blocks.h>
#include <block.h>
#include <r_tri_mesh.h>
#include "metric.h"

#define CD
#define INS
#define PS
#define SWIRL
#define BUOYANCY
//#define SWE
#define EXPLICIT
#define CNS
//#define CNS_EXPLICIT
//#define NONNEWTONIAN
#define KOMEGA

#define POD

#ifdef CD
#include "cd/tri_hp_cd.h"
#endif

#ifdef INS
#include "ins/tri_hp_ins.h"
#endif

#ifdef PS
#include "planestrain/tri_hp_ps.h"
#endif

#ifdef SWIRL
#include "swirl/tri_hp_swirl.h"
#endif

#ifdef BUOYANCY
#include "buoyancy/tri_hp_buoyancy.h"
#endif

#ifdef SWE
#include "swe/tri_hp_swe.h"
#endif

#ifdef POD
#include "pod/pod_simulate.h"
#include "pod/pod_generate.h"
#include "hp_boundary.h"
#endif

#ifdef EXPLICIT
#include "explicit/tri_hp_explicit.h"
#endif

#ifdef CNS
#include "cns/tri_hp_cns.h"
#endif

#ifdef CNS_EXPLICIT
#include "cns_explicit/tri_hp_cns_explicit.h"
#endif

#ifdef NONNEWTONIAN
#include "nonnewtonian/tri_hp_nonnewtonian.h"
#endif

#ifdef KOMEGA
#include "komega/tri_hp_komega.h"
#endif

class btype {
	public:
		const static int ntypes = 20;
		enum ids {r_tri_mesh,cd,ins,ps,swirl,buoyancy,komega,pod_ins_gen,pod_cd_gen,pod_cns_gen,pod_ins_sim,pod_cns_sim,pod_cd_sim,swe,explct,cns,cns_explicit,nonnewtonian,svv,cd_mapped};
		const static char names[ntypes][40];
		static int getid(const char *nin) {
			int i;
			for(i=0;i<ntypes;++i) 
				if (!strcmp(nin,names[i])) return(i);
			return(-1);
		}
};
const char btype::names[ntypes][40] = {"r_tri_mesh","cd","ins","ps","swirl","buoyancy","komega",
    "pod_ins_gen","pod_cd_gen","pod_cns_gen","pod_ins_sim","pod_cns_sim","pod_cd_sim","swe","explicit","cns","cns_explicit","nonnewtonian","svv","cd_mapped"};

multigrid_interface* block::getnewlevel(input_map& inmap) {
	std::string keyword,val,ibcname;
	std::istringstream data;
	int type;          

	/* FIND BLOCK TYPE */
	if (inmap.get(idprefix+"_type",val)) {
		type = btype::getid(val.c_str());
	}
	else {
		if (!inmap.get("blocktype",val)) {
			std::cerr << "couldn't find blocktype" << std::endl;
			sim::abort(__LINE__,__FILE__,&std::cerr);
		}
		type = btype::getid(val.c_str());
	}

	switch(type) {
		case btype::r_tri_mesh: {
			r_tri_mesh *temp = new r_tri_mesh();
			return(temp);
		}
#ifdef CD
		case btype::cd: {
			tri_hp_cd *temp = new tri_hp_cd();
			return(temp);
		}
#endif

#ifdef INS
		case btype::ins: {
			tri_hp_ins *temp = new tri_hp_ins();
			return(temp);
		}
#endif

#ifdef PS
		case btype::ps: {
			tri_hp_ps *temp = new tri_hp_ps();
			return(temp);
		}
#endif

#ifdef SWIRL
		case btype::swirl: {
			tri_hp_swirl *temp = new tri_hp_swirl();
			return(temp);
		}
#endif

#ifdef BUOYANCY
		case btype::buoyancy: {
			tri_hp_buoyancy *temp = new tri_hp_buoyancy();
			return(temp);
		}
#endif

#ifdef SWE
		case btype::swe: {
			tri_hp_swe *temp = new tri_hp_swe();
			return(temp);
		}
#endif
            
#ifdef KOMEGA
        case btype::komega: {
            tri_hp_komega *temp = new tri_hp_komega();
            return(temp);
        }
#endif

#if (defined(POD) && defined(CD))
		case btype::pod_cd_gen: {
			pod_generate<tri_hp_cd> *temp = new pod_generate<tri_hp_cd>();
			return(temp);
		}

		case btype::pod_cd_sim: {
			pod_simulate<tri_hp_cd> *temp = new pod_simulate<tri_hp_cd>();
			return(temp);
		}
#endif

#if (defined(POD) && defined(INS))
		case btype::pod_ins_gen: {
			pod_generate<tri_hp_ins> *temp = new pod_generate<tri_hp_ins>();
			return(temp);
		}

		case btype::pod_ins_sim: {
			pod_simulate<tri_hp_ins> *temp = new pod_simulate<tri_hp_ins>();
			return(temp);
		}
#endif

#if (defined(POD) && defined(CNS))
		case btype::pod_cns_gen: {
			pod_generate<tri_hp_cns> *temp = new pod_generate<tri_hp_cns>();
			return(temp);
		}
			
		case btype::pod_cns_sim: {
			pod_simulate<tri_hp_cns> *temp = new pod_simulate<tri_hp_cns>();
			return(temp);
		}
#endif
		
#if (defined(EXPLICIT)  && not(defined(petsc)) && not(defined(AXISYMMETRIC)))
		case btype::explct: {
			tri_hp_explicit *temp = new tri_hp_explicit();
			return(temp);
		}
#endif

#ifdef CNS
		case btype::cns: {
			tri_hp_cns *temp = new tri_hp_cns();
			return(temp);
		}
#endif
			
#if (defined(CNS_EXPLICIT) && not(defined(petsc)) && not(defined(AXISYMMETRIC)))
		case btype::cns_explicit: {
			tri_hp_cns_explicit *temp = new tri_hp_cns_explicit();
			return(temp);
		}
#endif

#ifdef NONNEWTONIAN
		case btype::nonnewtonian: {
			tri_hp_nonnewtonian *temp = new tri_hp_nonnewtonian();
			return(temp);
		}
#endif
			
#if (defined(POD) && defined(INS))
		case btype::svv: {
			svv_ins *temp = new svv_ins();
			return(temp);
		}
#endif
            
		default: {
			std::cerr << "unrecognizable block type: " <<  type << std::endl;
			r_tri_mesh *temp = new r_tri_mesh();
			return(temp);
		}
	} 

	return(0);
}


class metrictype {
    public:
        const static int ntypes = 3;
        enum ids {curved_boundary,all_curved,mapped};
        const static char names[ntypes][40];
        static int getid(const char *nin) {
            int i;
            for(i=0;i<ntypes;++i)
                if (!strcmp(nin,names[i])) return(i);
            return(-1);
        }
};
const char metrictype::names[ntypes][40] = {"curved_boundary","all_curved","mapped"};

unique_ptr<tri_hp::metric> tri_hp::getnewmetric(input_map &inmap) {
    std::string keyword,val,ibcname;
    std::istringstream data;
    int type;

    /* FIND BLOCK TYPE */
    keyword = gbl->idprefix+"_metric";
    if (!inmap.get(keyword,val)) {
        keyword = "metric";
        inmap.getwdefault(keyword,val,std::string(metrictype::names[0]));
    }
    type = metrictype::getid(val.c_str());
    

    switch(type) {
        case metrictype::curved_boundary: {
            return(make_unique<tri_hp::metric>(*this));
            break;
        }
        case metrictype::mapped: {
            return(make_unique<mapped_metric>(*this));
            break;
        }
        default: {
            *gbl->log << "unrecognized metric" << std::endl;
            sim::abort(__LINE__,__FILE__,&std::cerr);
        }
    }
    return(nullptr);
}



/* This routine waits for everyone to exit nicely */
void sim::finalize(int line,const char *file, std::ostream *log) {
    *log << "Exiting at line " << line << " of file " << file << std::endl;
#ifdef PTH
    pth_exit(NULL);
#endif
#ifdef BOOST
    throw boost::thread_interrupted();
#endif
#ifdef PTH
    pth_kill();
#endif
#ifdef petsc
    PetscFinalize();
#endif
#ifdef MPISRC
    MPI_Finalize();
#endif
    
    std::exit(0);
}

/* This routine forces everyone to die */
void sim::abort(int line,const char *file, std::ostream *log) {
    *log << "Exiting at line " << line << " of file " << file << std::endl;
    for (int b=0;b<blks.myblock;++b) {
        sim::blks.blk(b)->output("aborted_solution", block::display);
        sim::blks.blk(b)->output("aborted_solution", block::restart);
    }
#ifdef petsc
    PetscFinalize();
#endif
#ifdef MPI
    MPI_Abort(MPI_COMM_WORLD,1);
#endif
    
    /* Terminates all threads */
    std::exit(1);
}

