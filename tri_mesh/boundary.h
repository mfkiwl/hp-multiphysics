#ifndef _boundary_h_
#define _boundary_h_

/*
 *  boundary.h
 *  mesh
 *
 *  Created by Brian Helenbrook on Fri Jun 07 2002.
 *  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
 *
 */
#include <stdio.h>
#include <input_map.h>
#include <symbolic_function.h>
#include <float.h>
#include "blocks.h"

using namespace blitz;

#ifdef MPISRC
#include <mpi.h>
#endif

#ifdef SINGLE
#define FLT float
#define EPSILON FLT_EPSILON
#else
#ifndef FLT
#define FLT double
#define EPSILON DBL_EPSILON
#endif
#endif

//#define MPDEBUG

/** \brief Template class to make a communciation boundary
 *
 * \ingroup boundary
 * Contains variables and routines for communciation boundaries.
 * Can be applied to vrtx_bdry or edge_bdry to make specific type
 */
template<class BASE,class MESH> class comm_bdry : public BASE {
protected:
    static const int maxmatch = 8; //!< Max number of communication mathces
    static const int maxgroup = 5; //!< Max number of different communication groups
    bool first; //!< For master-slave communication. Only one boundary of matching boundaries is first
    int groupmask;  //!< To make groups that only communicate in restricted situations group 0 all, group 1 all phased, group 2 partitions, group 3 manifolds
    TinyVector<int,maxgroup> maxphase; //!<  For phased symmetric message passing for each group
    TinyMatrix<int,maxgroup,maxmatch> phase;  //!< To set-up staggered sequence of symmetric passes for each group (-1 means skip)
    int& msg_phase(int grp, int match) {return(phase(grp,match));} //!< virtual accessor
    int buffsize; //!< Size in bytes of buffer
    int msgsize; //!< Outgoing size
    boundary::msg_type msgtype; //!< Outgoing type
    bool use_one_send_buffer;
    
    /** Different types of matching boundaries,
     * local is same processor same thread
     * mpi is different processor
     * someday have threads but not yet
     */
#ifdef MPISRC
    enum matchtype {local, mpi};
#else
    enum matchtype {local};
#endif
    int nmatch; //!< Number of matching boundaries
    int& nmatches() {return(nmatch);} //!< virtual accessor
    TinyVector<matchtype,maxmatch> mtype; //!< Local or mpi or ?
    TinyVector<boundary *,maxmatch> local_match; //!< Pointers to local matches
    TinyVector<std::string,maxmatch> match_names;
    TinyVector<int,maxmatch> snd_tags; //!< Identifies each connection uniquely
    TinyVector<int,maxmatch> rcv_tags; //!< Identifies each connection uniquely
    TinyVector<void *,maxmatch> sndbuf; //!< Raw memory to store incoming messages
    TinyVector<void *,maxmatch> rcvbuf; //!< Raw memory to store incoming messages
    TinyVector<Array<FLT,1>,maxmatch> fsndbufarray; //!< Access to incoming message buffer for floats
    TinyVector<Array<int,1>,maxmatch> isndbufarray; //!< Access to incoming message buffer for ints
    TinyVector<Array<FLT,1>,maxmatch> frcvbufarray; //!< Access to incoming message buffer for floats
    TinyVector<Array<int,1>,maxmatch> ircvbufarray; //!< Access to incoming message buffer for ints
    
#ifdef MPISRC
    TinyVector<int,maxmatch> mpi_match; //!< Processor numbers for mpi
    TinyVector<MPI_Request,maxmatch> mpi_rcvrqst; //!< Identifier returned from mpi to monitor success of recv
    TinyVector<MPI_Request,maxmatch> mpi_sndrqst; //!< Identifier returned from mpi to monitor success of send
#endif
    
public:
    comm_bdry(int inid, MESH &xin) : BASE(inid,xin), first(1), groupmask(0x3), buffsize(0), nmatch(0), maxphase(0), use_one_send_buffer(true) {
        for (int m=0;m<maxmatch;++m) {
            sndbuf(m) = NULL;
            rcvbuf(m) = NULL; // So I know they haven't been allocated
        }
        phase = 0;
    }
    comm_bdry(const comm_bdry<BASE,MESH> &inbdry, MESH& xin) : BASE(inbdry,xin), first(inbdry.first), groupmask(inbdry.groupmask), buffsize(0), nmatch(0),
    maxphase(inbdry.maxphase), phase(inbdry.phase), use_one_send_buffer(inbdry.use_one_send_buffer) {
        /* COPY THESE, BUT WILL HAVE TO BE RESET TO NEW MATCHING SIDE */
        first = true; // Findmatch sets this
        mtype = inbdry.mtype;
        local_match = inbdry.local_match;
        match_names = inbdry.match_names;
        snd_tags = inbdry.snd_tags;
        rcv_tags = inbdry.rcv_tags;
#ifdef MPISRC
        mpi_match = inbdry.mpi_match;
#endif
        for (int m=0;m<maxmatch;++m) {
            sndbuf(m) = NULL;
            rcvbuf(m) = NULL; // So I know they haven't been allocated
        }
        return;
    }
    
    comm_bdry<BASE,MESH>* create(MESH &xin) const {return(new comm_bdry<BASE,MESH>(*this,xin));}
    bool is_comm() {return(true);}
    bool& is_frst() {return(first);}
    int& matchphase(boundary::groups group, int matchnum) {return(phase(group,matchnum));}
    bool is_local(int matchnum) {return(mtype(matchnum) == local);}
    bool in_group(int grp) {return(((1<<grp)&groupmask));}
    int& sndsize() {return(msgsize);}
    boundary::msg_type& sndtype() {return(msgtype);}
    void match_name(int m, std::string& name) {name = match_names(m);}
    bool& one_send_buf() {return(use_one_send_buffer);}
    int& isndbuf(int indx) {return(isndbufarray(0)(indx));}
    FLT& fsndbuf(int indx) {return(fsndbufarray(0)(indx));}
    int& isndbuf(int m, int indx) {return(isndbufarray(m)(indx));}
    FLT& fsndbuf(int m, int indx) {return(fsndbufarray(m)(indx));}
    int& ircvbuf(int m,int indx) {return(ircvbufarray(m)(indx));}
    FLT& frcvbuf(int m,int indx) {return(frcvbufarray(m)(indx));}
    
    void resize_buffers(int nfloats) {
        buffsize = nfloats*sizeof(FLT);
        
        for (int m=0;m<nmatch;++m) {
            if (sndbuf(m) != NULL) free(sndbuf(m));
            sndbuf(m) = malloc(buffsize);
            if(!sndbuf(m)) {
                std::cerr << "Could not allocate memory! " << BASE::idprefix << ' ' << nfloats << std::endl;
                sim::abort(__LINE__,__FILE__,BASE::x.gbl->log);
            }
            Array<FLT,1> temp1(static_cast<FLT *>(sndbuf(m)), buffsize/sizeof(FLT), neverDeleteData);
            fsndbufarray(m).reference(temp1);
            Array<int,1> temp2(static_cast<int *>(sndbuf(m)), buffsize/sizeof(int), neverDeleteData);
            isndbufarray(m).reference(temp2);
            
            if (rcvbuf(m) != NULL) free(rcvbuf(m));
            rcvbuf(m) = malloc(buffsize);
            if(!rcvbuf(m)) {
                std::cerr << "Could not allocate memory!" << BASE::idprefix << ' ' << nfloats << std::endl;
                sim::abort(__LINE__,__FILE__,BASE::x.gbl->log);
            }
            Array<FLT,1> temp3(static_cast<FLT *>(rcvbuf(m)), buffsize/sizeof(FLT), neverDeleteData);
            frcvbufarray(m).reference(temp3);
            Array<int,1> temp4(static_cast<int *>(rcvbuf(m)), buffsize/sizeof(int), neverDeleteData);
            ircvbufarray(m).reference(temp4);
        }
    }
    
    void alloc(int nels) {
        BASE::alloc(nels);
        resize_buffers(nels*3);
    }
    
    void add_to_group(int grp) {
        groupmask = groupmask|(1<<grp);
        for(int m=0; m < maxmatch; ++m)
            phase(grp,m) = 0;
        maxphase(grp) = 0;
    }
    
    void add_to_group(int grp, std::vector<int> phaselist) {
        groupmask = groupmask|(1<<grp);
        for(int m=0; m < phaselist.size(); ++m) {
            phase(grp,m) = phaselist[m];
            maxphase(grp) = MAX(maxphase(grp),phase(grp,m));
        }
    }
    
    void init(input_map& inmap) {
        int j,m;
        std::string keyword,gs,ps;
        std::map<std::string,std::string>::const_iterator mi;
        std::istringstream gd,pd;
        std::ostringstream nstr;
        
        BASE::init(inmap);
                
        first = true;
        if (inmap.get(BASE::idprefix +"_first",first)) {
            *BASE::x.gbl->log << "#Setting first flag for " << BASE::idprefix << std::endl;
            *BASE::x.gbl->log << "#This only works if all other matches are set to false" << std::endl;
        }
        
        /* SET GROUP MEMBERSHIP FLAGS */
        /* For each group and each bdry match there is a phase */
        if (inmap.getline(BASE::idprefix + "_group",gs)) {
            groupmask = 0;
            gd.str(gs);
            while(gd >> m) {
                add_to_group(m);  // ADD AS UNPHASED GROUP FIRST
            }
        }
        
        /* SKIP FIRST GROUP (NEVER PHASED) */
        for(int m=1;m<maxgroup;++m) {
            if (in_group(m)) {
                /* Load phase list for this group */
                nstr.str("");
                nstr << BASE::idprefix << "_phase" << m << std::flush;
                if (inmap.getline(nstr.str(),ps)) {
                    std::vector<int> phaselist;
                    pd.str(ps);
                    while(pd >> j) {
                        phaselist.push_back(j);
                    }
                    add_to_group(m,phaselist);
                }
            }
        }
    }
    
    int local_cnnct(boundary *bin, int snd_tag, int rcv_tag) {
        if (bin->idnum == BASE::idnum) {
            mtype(nmatch) = local;
            local_match(nmatch) = bin;
            match_names(nmatch) = bin->idprefix;
            snd_tags(nmatch) = snd_tag;
            rcv_tags(nmatch) = rcv_tag;
            sndbuf(nmatch) = malloc(buffsize);
            if(!sndbuf(nmatch)) {
                std::cerr << "Could not allocate memory! " << BASE::idprefix << ' ' << buffsize << std::endl;
                sim::abort(__LINE__,__FILE__,BASE::x.gbl->log);
            }
            Array<FLT,1> temp1(static_cast<FLT *>(sndbuf(nmatch)), buffsize/sizeof(FLT), neverDeleteData);
            fsndbufarray(nmatch).reference(temp1);
            Array<int,1> temp2(static_cast<int *>(sndbuf(nmatch)), buffsize/sizeof(int), neverDeleteData);
            isndbufarray(nmatch).reference(temp2);
            
            rcvbuf(nmatch) = malloc(buffsize);
            if(!rcvbuf(nmatch)) {
                std::cerr << "Could not allocate memory! " << BASE::idprefix << ' ' << buffsize << std::endl;
                sim::abort(__LINE__,__FILE__,BASE::x.gbl->log);
            }
            Array<FLT,1> temp3(static_cast<FLT *>(rcvbuf(nmatch)), buffsize/sizeof(FLT), neverDeleteData);
            frcvbufarray(nmatch).reference(temp3);
            Array<int,1> temp4(static_cast<int *>(rcvbuf(nmatch)), buffsize/sizeof(int), neverDeleteData);
            ircvbufarray(nmatch).reference(temp4);
            ++nmatch;
            return(0);
        }
        *BASE::x.gbl->log << "error: not local match" << BASE::idnum << bin->idnum << std::endl;
        return(1);
    }
    
#ifdef MPISRC
    int mpi_cnnct(int nproc, int snd_tag, int rcv_tag, std::string name) {
        mtype(nmatch) = mpi;
        mpi_match(nmatch) = nproc;
        match_names(nmatch) = name;
        snd_tags(nmatch) = snd_tag;
        rcv_tags(nmatch) = rcv_tag;
        sndbuf(nmatch) = malloc(buffsize);
        if(!sndbuf(nmatch)) {
            std::cerr << "Could not allocate memory! "  << BASE::idprefix << ' ' << buffsize << std::endl;
            sim::abort(__LINE__,__FILE__,BASE::x.gbl->log);
        }
        Array<FLT,1> temp1(static_cast<FLT *>(sndbuf(nmatch)), buffsize/sizeof(FLT), neverDeleteData);
        fsndbufarray(nmatch).reference(temp1);
        Array<int,1> temp2(static_cast<int *>(sndbuf(nmatch)), buffsize/sizeof(int), neverDeleteData);
        isndbufarray(nmatch).reference(temp2);
        
        rcvbuf(nmatch) = malloc(buffsize);
        if(!rcvbuf(nmatch)) {
            std::cerr << "Could not allocate memory!"  << BASE::idprefix << ' ' << buffsize << std::endl;
            sim::abort(__LINE__,__FILE__,BASE::x.gbl->log);
        }
        Array<FLT,1> temp3(static_cast<FLT *>(rcvbuf(nmatch)), buffsize/sizeof(FLT), neverDeleteData);
        frcvbufarray(nmatch).reference(temp3);
        Array<int,1> temp4(static_cast<int *>(rcvbuf(nmatch)), buffsize/sizeof(int), neverDeleteData);
        ircvbufarray(nmatch).reference(temp4);
        ++nmatch;
        return(0);
    }
#endif
    
    /* MECHANISM FOR SYMMETRIC SENDING */
    void comm_prepare(boundary::groups grp, int phi, boundary::comm_type type) {
        int m;
        int nrecvs_to_post;
        int nsends_to_post;
        
#ifdef MPISRC
        int err;
#endif
        
        if (!in_group(grp)) return;
        
        /* SWITCHES FOR MASTER_SLAVE */
        switch(type) {
            case(boundary::master_slave): {
                if (first) {
                    nsends_to_post = nmatch;
                    nrecvs_to_post = 0;
                }
                else {
                    nrecvs_to_post = 1;
                    nsends_to_post = 0;
                }
                break;
            }
            case(boundary::slave_master): {
                if (!first) {
                    nsends_to_post = 1;
                    nrecvs_to_post = 0;
                }
                else {
                    nrecvs_to_post = nmatch;
                    nsends_to_post = 0;
                }
                break;
            }
            default: { // SYMMETRIC
                nrecvs_to_post = nmatch;
                nsends_to_post = nmatch;
                break;
            }
        }
        
#ifdef MPDEBUG
        if (nsends_to_post) {
            *BASE::x.gbl->log << "preparing to send these messages from "  << BASE::idprefix << "with type " << type << std::endl;
            switch(sndtype()) {
                case(boundary::flt_msg): {
                    if (use_one_send_buf) {
                        *BASE::x.gbl->log << fsndbufarray(Range(0,sndsize()-1)) << std::endl;
                    }
                    else {
                        for(int m=0;m<nmatch;++m) {
                            *BASE::x.gbl->log << fsndbufarray(m,Range(0,sndsize()-1)) << std::endl;
                        }
                    }
                    break;
                }
                case(boundary::int_msg): {
                    if (use_one_send_buf) {
                        *BASE::x.gbl->log << isndbufarray(Range(0,sndsize()-1)) << std::endl;
                    }
                    else {
                        for(int m=0;m<nmatch;++m) {
                            *BASE::x.gbl->log << isndbufarray(m,Range(0,sndsize()-1)) << std::endl;
                        }
                    }
                    
                    break;
                }
            }
        }
#endif
        
        /* MPI POST RECEIVES FIRST */
        for(m=0;m<nrecvs_to_post;++m) {
            if (phi != phase(grp,m)) continue;
            
            switch(mtype(m)) {
                case(local):
                    /* NOTHING TO DO FOR LOCAL RECEIVES */
                    break;
#ifdef MPISRC
                case(mpi):
                    switch(sndtype()) {
                        case(boundary::flt_msg): {
#ifdef SINGLE
                            err = MPI_Irecv(&frcvbuf(m,0), buffsize/sizeof(FLT), MPI_FLOAT,
                                            mpi_match(m), rcv_tags(m), MPI_COMM_WORLD, &mpi_rcvrqst(m));
#else
                            err = MPI_Irecv(&frcvbuf(m,0), buffsize/sizeof(FLT), MPI_DOUBLE,
                                            mpi_match(m), rcv_tags(m), MPI_COMM_WORLD, &mpi_rcvrqst(m));
#endif
                            break;
                        }
                        case(boundary::int_msg): {
                            err = MPI_Irecv(&ircvbuf(m,0), buffsize/sizeof(int), MPI_INT,
                                            mpi_match(m), rcv_tags(m), MPI_COMM_WORLD, &mpi_rcvrqst(m));
                            break;
                        }
                    }
#endif
            }
        }
        
        /* LOCAL POST SENDS FIRST */
        for(m=0;m<nsends_to_post;++m) {
            if (phi != phase(grp,m)) continue;
            
            switch(mtype(m)) {
                case(local):
                    sim::blks.notify_change(snd_tags(m),true);
                    break;
#ifdef MPISRC
                case(mpi):
                    /* NOTHING TO DO FOR MPI SENDS */
                    break;
#endif
            }
        }
    }
    
    void comm_exchange(boundary::groups grp, int phi, boundary::comm_type type) {
        int i,m;
#ifdef MPISRC
        int err;
#endif
        int nrecvs_to_post = nmatch, nsends_to_post = nmatch;
        
        if (!in_group(grp)) return;
        
        switch(type) {
            case(boundary::master_slave): {
                if (!first) {
                    nrecvs_to_post = 1;
                    nsends_to_post = 0;
                }
                else {
                    nrecvs_to_post = 0;
                    nsends_to_post = nmatch;
                }
                break;
            }
            case(boundary::slave_master): {
                if (first) {
                    nrecvs_to_post = nmatch;
                    nsends_to_post = 0;
                }
                else {
                    nrecvs_to_post = 0;
                    nsends_to_post = 1;
                }
                break;
            }
            case(boundary::symmetric): {
                nrecvs_to_post = nmatch;
                nsends_to_post = nmatch;
                break;
            }
        }
        
        
        /* LOCAL PASSES */
        for(m=0;m<nrecvs_to_post;++m) {
            if (phi != phase(grp,m) || mtype(m) != local) continue;
            
            sim::blks.waitforslot(rcv_tags(m),true);
            
            switch(sndtype()) {
                case(boundary::flt_msg): {
                    if (use_one_send_buffer) {
                        for(i=0;i<local_match(m)->sndsize();++i)
                            frcvbuf(m,i) = local_match(m)->fsndbuf(i);
                    }
                    else {
                        for(i=0;i<local_match(m)->sndsize();++i)
                            frcvbuf(m,i) = local_match(m)->fsndbuf(m,i);
                    }
                    break;
                }
                case(boundary::int_msg): {
                    if (use_one_send_buffer) {
                        for(i=0;i<local_match(m)->sndsize();++i)
                            ircvbuf(m,i) = local_match(m)->isndbuf(i);
                    }
                    else {
                        for(i=0;i<local_match(m)->sndsize();++i)
                            ircvbuf(m,i) = local_match(m)->isndbuf(m,i);
                    }
                    
                    break;
                }
            }
            sim::blks.notify_change(rcv_tags(m),false);
        }
        
#ifdef MPISRC
        /* MPI PASSES */
        for(m=0;m<nsends_to_post;++m) {
            if (phi != phase(grp,m) || mtype(m) != mpi) continue;
            
            switch(sndtype()) {
                case(boundary::flt_msg): {
                    FLT *psndbuf = &fsndbufarray(m)(0);
                    if (use_one_send_buffer) {
                        psndbuf = &fsndbufarray(0)(0);
                    }
#ifdef SINGLE
                    err = MPI_Isend(psndbuf, msgsize, MPI_FLOAT,
                                    mpi_match(m), snd_tags(m), MPI_COMM_WORLD, &mpi_sndrqst(m));
#else
                    err = MPI_Isend(psndbuf, msgsize, MPI_DOUBLE,
                                    mpi_match(m), snd_tags(m), MPI_COMM_WORLD, &mpi_sndrqst(m));
#endif
                    break;
                }
                case(boundary::int_msg): {
                    int *psndbuf = &isndbufarray(m)(0);
                    if (use_one_send_buffer) {
                        psndbuf = &isndbufarray(0)(0);
                    }
                    err = MPI_Isend(psndbuf, msgsize, MPI_INT,
                                    mpi_match(m), snd_tags(m), MPI_COMM_WORLD, &mpi_sndrqst(m));
                    break;
                }
            }
        }
#endif
        return;
    }
    
    int comm_wait(boundary::groups grp, int phi, boundary::comm_type type) {
        int nrecvs_to_post;
        int nsends_to_post;
        
        if (!in_group(grp)) return(1);
        
        switch(type) {
            case(boundary::master_slave): {
                if (first) {
                    nrecvs_to_post = 0;
                    nsends_to_post = nmatch;
                }
                else {
                    nrecvs_to_post = 1;
                    nsends_to_post = 0;
                }
                break;
            }
                
            case(boundary::slave_master): {
                if (!first) {
                    nrecvs_to_post = 0;
                    nsends_to_post = 1;
                }
                else {
                    nrecvs_to_post = nmatch;
                    nsends_to_post = 0;
                }
                break;
            }
                
            default: {  // SYMMETRIC
                nrecvs_to_post = nmatch;
                nsends_to_post = nmatch;
                break;
            }
        }
        
        for(int m=0;m<nsends_to_post;++m) {
            if (phi != phase(grp,m)) continue;
            
            switch(mtype(m)) {
                case(local): {
                    sim::blks.waitforslot(snd_tags(m),false);
                    break;
                }
#ifdef MPISRC
                case(mpi): {
                    MPI_Status status;
                    MPI_Wait(&mpi_sndrqst(m), &status);
                    break;
                }
#endif
            }
        }
        
        
        for(int m=0;m<nrecvs_to_post;++m) {
            if (phi != phase(grp,m)) continue;
            
            switch(mtype(m)) {
                case(local): {
                    break;
                }
#ifdef MPISRC
                case(mpi): {
                    MPI_Status status;
                    MPI_Wait(&mpi_rcvrqst(m), &status);
                    break;
                }
#endif
            }
        }
        
        /* ONE MEANS FINISHED 0 MEANS MORE TO DO */
        return((phi-maxphase(grp) >= 0 ? 1 : 0));
    }
    
    bool comm_finish(boundary::groups grp, int phi, boundary::comm_type type, boundary::operation op) {
        
        if (!in_group(grp)) return(false);
        
        switch(type) {
            case(boundary::slave_master): {
                if (!is_frst()) return(false);
            }
                
            case(boundary::master_slave): {
                if (is_frst() || (phase(grp,0) != phi)) return(false);
                
#ifdef MPDEBUG
                *BASE::x.gbl->log << "finish master_slave"  << BASE::idprefix << std::endl;
#endif
                switch(sndtype()) {
                    case(boundary::int_msg):
                        for(int j=0;j<sndsize();++j) {
                            isndbuf(j) = ircvbuf(0,j);
                        }
#ifdef MPDEBUG
                        *BASE::x.gbl->log << isndbufarray(0)(Range(0,sndsize()-1)) << std::endl;
#endif
                        break;
                        
                    case(boundary::flt_msg):
                        for(int j=0;j<sndsize();++j) {
                            fsndbuf(j) = frcvbuf(0,j);
                        }
#ifdef MPDEBUG
                        *BASE::x.gbl->log << fsndbufarray(0)(Range(0,sndsize()-1)) << std::endl;
#endif
                        break;
                }
                return(true);
                
            }
                
            default: {
                switch(sndtype()) {
                    case(boundary::flt_msg): {
                        switch(op) {
                            case(boundary::average): {
                                int matches = 1;
                                for(int m=0;m<nmatch;++m) {
                                    if (phase(grp,m) != phi) continue;
                                    
                                    ++matches;
                                    for(int j=0;j<sndsize();++j) {
                                        fsndbuf(j) += frcvbuf(m,j);
                                    }
                                }
                                if (matches > 1 ) {
                                    FLT mtchinv = 1./matches;
                                    for(int j=0;j<sndsize();++j)
                                        fsndbuf(j) *= mtchinv;
#ifdef MPDEBUG
                                    *BASE::x.gbl->log << "finish average"  << BASE::idprefix << std::endl;
                                    *BASE::x.gbl->log << fsndbufarray(0)(Range(0,sndsize()-1)) << std::endl;
#endif
                                    return(true);
                                }
                                return(false);
                            }
                                
                            case(boundary::sum): {
                                int matches = 1;
                                
                                for(int m=0;m<nmatch;++m) {
                                    if (phase(grp,m) != phi) continue;
                                    
                                    ++matches;
                                    for(int j=0;j<sndsize();++j) {
                                        fsndbuf(j) += frcvbuf(m,j);
                                    }
                                }
                                if (matches > 1 ) {
#ifdef MPDEBUG
                                    *BASE::x.gbl->log << "finish sum"  << BASE::idprefix << std::endl;
                                    *BASE::x.gbl->log << fsndbufarray(0)(Range(0,sndsize()-1)) << std::endl;
#endif
                                    return(true);
                                    
                                }
                                return(false);
                            }
                                
                            case(boundary::maximum): {
                                int matches = 1;
                                
                                for(int m=0;m<nmatch;++m) {
                                    if (phase(grp,m) != phi) continue;
                                    
                                    ++matches;
                                    
                                    for(int j=0;j<sndsize();++j) {
                                        fsndbuf(j) = MAX(fsndbuf(j),frcvbuf(m,j));
                                    }
                                }
                                
                                if (matches > 1 ) {
#ifdef MPDEBUG
                                    *BASE::x.gbl->log << "finish max"  << BASE::idprefix << std::endl;
                                    *BASE::x.gbl->log << fsndbufarray(0)(Range(0,sndsize()-1)) << std::endl;
#endif
                                    return(true);
                                }
                                return(false);
                            }
                                
                            case(boundary::minimum): {
                                int matches = 1;
                                
                                for(int m=0;m<nmatch;++m) {
                                    if (phase(grp,m) != phi) continue;
                                    
                                    ++matches;
                                    
                                    for(int j=0;j<sndsize();++j) {
                                        fsndbuf(j) = MIN(fsndbuf(j),frcvbuf(m,j));
                                    }
                                }
                                
                                if (matches > 1 ) {
#ifdef MPDEBUG
                                    *BASE::x.gbl->log << "finish min"  << BASE::idprefix << std::endl;
                                    *BASE::x.gbl->log << fsndbufarray(0)(Range(0,sndsize()-1)) << std::endl;
#endif
                                    return(true);
                                    
                                }
                                return(false);
                            }
                                
                            case(boundary::replace): {
                                int matches = 1;
                                
                                for(int m=0;m<nmatch;++m) {
                                    if (phase(grp,m) != phi) continue;
                                    
                                    ++matches;
                                    
                                    for(int j=0;j<sndsize();++j) {
                                        fsndbuf(j) = frcvbuf(m,j);
                                    }
                                }
                                
                                if (matches > 1 ) {
#ifdef MPDEBUG
                                    *BASE::x.gbl->log << "finish min"  << BASE::idprefix << std::endl;
                                    *BASE::x.gbl->log << fsndbufarray(0)(Range(0,sndsize()-1)) << std::endl;
#endif
                                    return(true);
                                    
                                }
                                return(false);
                            }
                        }
                    }
                        
                    case(boundary::int_msg): {
                        switch(op) {
                            case(boundary::average): {
                                *BASE::x.gbl->log << "averaging with integer messages?" << std::endl;
                                sim::abort(__LINE__,__FILE__,BASE::x.gbl->log);
                            }
                                
                            case(boundary::sum): {
                                int matches = 1;
                                
                                for(int m=0;m<nmatch;++m) {
                                    if (phase(grp,m) != phi) continue;
                                    
                                    ++matches;
                                    for(int j=0;j<sndsize();++j) {
                                        isndbuf(j) += ircvbuf(m,j);
                                    }
                                }
                                if (matches > 1 ) {
#ifdef MPDEBUG
                                    
                                    *BASE::x.gbl->log << "finish sum"  << BASE::idprefix << std::endl;
                                    *BASE::x.gbl->log << isndbufarray(0)(Range(0,sndsize()-1)) << std::endl;
#endif
                                    return(true);
                                }
                                return(false);
                            }
                                
                            case(boundary::maximum): {
                                int matches = 1;
                                
                                for(int m=0;m<nmatch;++m) {
                                    if (phase(grp,m) != phi) continue;
                                    
                                    ++matches;
                                    
                                    for(int j=0;j<sndsize();++j) {
                                        isndbuf(j) = MAX(isndbuf(j),ircvbuf(m,j));
                                    }
                                }
                                
                                if (matches > 1 ) {
#ifdef MPDEBUG
                                    *BASE::x.gbl->log << "finish max"  << BASE::idprefix << std::endl;
                                    *BASE::x.gbl->log << isndbufarray(0)(Range(0,sndsize()-1)) << std::endl;
#endif
                                    return(true);
                                }
                                return(false);
                            }
                                
                            case(boundary::minimum): {
                                int matches = 1;
                                
                                for(int m=0;m<nmatch;++m) {
                                    if (phase(grp,m) != phi) continue;
                                    
                                    ++matches;
                                    
                                    for(int j=0;j<sndsize();++j) {
                                        isndbuf(j) = MIN(isndbuf(j),ircvbuf(m,j));
                                    }
                                }
                                
                                if (matches > 1 ) {
#ifdef MPDEBUG
                                    *BASE::x.gbl->log << "finish min"  << BASE::idprefix << std::endl;
                                    *BASE::x.gbl->log << isndbufarray(0)(Range(0,sndsize()-1)) << std::endl;
#endif
                                    return(true);
                                }
                                return(false);
                            }
                                
                            case(boundary::replace): {
                                int matches = 1;
                                
                                for(int m=0;m<nmatch;++m) {
                                    if (phase(grp,m) != phi) continue;
                                    
                                    ++matches;
                                    
                                    for(int j=0;j<sndsize();++j) {
                                        isndbuf(j) = ircvbuf(m,j);
                                    }
                                }
                                
                                if (matches > 1 ) {
#ifdef MPDEBUG
                                    *BASE::x.gbl->log << "finish replace"  << BASE::idprefix << std::endl;
                                    *BASE::x.gbl->log << isndbufarray(0)(Range(0,sndsize()-1)) << std::endl;
#endif
                                    return(true);
                                }
                                return(false);
                            }
                        }
                    }
                }
            }
        }
        
        return(false);
    }
};

/* Generic interface to allow rigid body rotation and translation of boundaries */
class rigid_movement_interface2D {
public:
    FLT theta;
    TinyVector<FLT,2> pos;
    
    
    rigid_movement_interface2D() : theta(0.0) {
        pos = 0.0;
    }
    rigid_movement_interface2D(const rigid_movement_interface2D &to_copy) : theta(to_copy.theta) {
        pos = to_copy.pos;
    }
    rigid_movement_interface2D* create() const {return(new rigid_movement_interface2D(*this));}
    void init(input_map& inmap,std::string idprefix) {
        inmap.getwdefault(idprefix+"_theta",theta,0.0);
        theta = theta*M_PI/180.0;
        FLT ctr_dflt[2] = {0.0, 0.0};
        inmap.getwdefault(idprefix+"_center",pos.data(),2,ctr_dflt);
    }
    void to_geometry_frame(TinyVector<FLT,2>& pt) {
        pt -= pos;
        FLT temp = pt[0]*cos(theta) +pt[1]*sin(theta);
        pt[1] = -pt[0]*sin(theta) +pt[1]*cos(theta);
        pt[0] = temp;
    }
    void to_physical_frame(TinyVector<FLT,2>& pt) {
        FLT temp = pt[0]*cos(-theta) +pt[1]*sin(-theta);
        pt[1] = -pt[0]*sin(-theta) +pt[1]*cos(-theta);
        pt[0] = temp;
        pt += pos;
    }
    void rotate_vector(TinyVector<FLT,2>& vec) {
        FLT temp = vec[0]*cos(-theta) +vec[1]*sin(-theta);
        vec[1] = -vec[0]*sin(-theta) +vec[1]*cos(-theta);
        vec[0] = temp;
    }
};


/* GENERIC TEMPLATE FOR SHAPES */
template<int ND> class geometry {
protected:
    virtual FLT hgt(TinyVector<FLT,ND> &pt, FLT time = 0.0) {return(0.0);}
    virtual FLT dhgt(int dir, TinyVector<FLT,ND> &pt, FLT time = 0.0) {return(1.0);}
    
public:
    // virtual geometry* create() const {return(new geometry);}
    virtual int mvpttobdry(TinyVector<FLT,ND> &pt, FLT time) {
        int iter,n;
        FLT mag, delt_dist;
        TinyVector<FLT,ND> deriv;
        
        /* FOR AN ANALYTIC SURFACE */
        iter = 0;
        do {
            mag = 0.0;
            for(n=0;n<ND;++n) {
                deriv(n) = dhgt(n,pt,time);
                mag += deriv(n)*deriv(n);
            }
            delt_dist = -hgt(pt,time)/mag;
            
            for(n=0;n<ND;++n)
                pt(n) += delt_dist*deriv(n);
            
            if (++iter > 100) {
                std::cout << "Warning: curved iterations exceeded for symbolic curved boundary " << pt(0) << ' ' << pt(1) << "Ratio to target error level " << fabs(delt_dist)/(10.*EPSILON) << '\n';  // FIXME:  NEED TO FIX
                return(1);
            }
        } while (fabs(delt_dist) > 20.*EPSILON);
        
        return(0);
    }
    virtual void bdry_normal(TinyVector<FLT,ND> pt, FLT time, TinyVector<FLT,ND>& norm) {
        FLT mag = 0.0;
        for(int n=0;n<ND;++n) {
            norm(n) = dhgt(n,pt,time);
            mag += norm(n)*norm(n);
        }
        norm /= sqrt(mag);
        
        return;
    }
    virtual void init(input_map& inmap, std::string idprefix, std::ostream& log) {}
    virtual ~geometry() {}
};


template<int ND> class symbolic_point : public geometry<ND> {
protected:
    TinyVector<symbolic_function<0>,ND> loc;
public:
    symbolic_point() : geometry<ND>() {}
    symbolic_point(const symbolic_point& tgt) : geometry<ND>(tgt), loc(tgt.loc) {}
    
    void init(input_map& inmap, std::string idprefix, std::ostream& log) {
        geometry<ND>::init(inmap,idprefix,log);
        
        ostringstream nstr;
        
        for(int n=0;n<ND;++n) {
            nstr.str("");
            nstr << "_locx" << n;
            
            if (inmap.find(idprefix + nstr.str()) != inmap.end()) {
                loc(n).init(inmap,idprefix+nstr.str());
            }
            else {
                log << "couldn't find shape function " << idprefix << nstr.str() << std::endl;
                sim::abort(__LINE__,__FILE__,&log);
            }
        }
    }
    
    int mvpttobdry(TinyVector<FLT,ND> &pt, FLT time) {
        for(int i = 0; i < ND; ++i)
            pt(i) = loc(i).Eval(time);
        return 0;
    }
};

template<int ND> class symbolic_shape : public geometry<ND> {
protected:
    symbolic_function<ND> h;
    TinyVector<symbolic_function<ND>,ND> dhdx;
    
    FLT hgt(TinyVector<FLT,ND> &pt, FLT time = 0.0) {
        return(h.Eval(pt,time));
    }
    FLT dhgt(int dir, TinyVector<FLT,ND> &pt, FLT time = 0.0) {
        return(dhdx(dir).Eval(pt,time));
    }
    
public:
    symbolic_shape() : geometry<ND>() {}
    symbolic_shape(const symbolic_shape& tgt) : geometry<ND>(tgt), h(tgt.h) {
        for(int n=0; n<ND;++n) {
            dhdx(n) = tgt.dhdx(n);
        }
    }
    
    void init(input_map& inmap, std::string idprefix, std::ostream& log) {
        geometry<ND>::init(inmap,idprefix,log);
        if (inmap.find(idprefix +"_h") != inmap.end()) {
            h.init(inmap,idprefix+"_h");
        }
        else {
            log << "couldn't find shape function " << idprefix << "_h" << std::endl;
            sim::abort(__LINE__,__FILE__,&log);
        }
        
        ostringstream nstr;
        
        for(int n=0;n<ND;++n) {
            nstr.str("");
            nstr << "_dhdx" << n;
            if (inmap.find(idprefix +nstr.str()) != inmap.end()) {
                dhdx(n).init(inmap,idprefix+nstr.str());
            }
            else {
                log << "couldn't find shape function for " << idprefix << nstr.str() << std::endl;
                sim::abort(__LINE__,__FILE__,&log);
            }
            
        }
        
    }
    
};


class circle : public geometry<2> {
public:
    // Position is kept in rigid class
    FLT radius;
    TinyVector<FLT,2> ctr;
    FLT hgt(TinyVector<FLT,2>& pt,FLT time = 0.0) {
        return(radius*radius -pt[0]*pt[0] -pt[1]*pt[1]);
    }
    FLT dhgt(int dir, TinyVector<FLT,2>& pt,FLT time = 0.0) {
        return(-2.*(pt[dir]));
    }
    
    circle() : geometry<2>(), radius(0.5), ctr(0.) {}
    circle(const circle &inbdry) : geometry<2>(inbdry), radius(inbdry.radius), ctr(inbdry.ctr) {}
    circle* create() const {return(new circle(*this));}
    
    void init(input_map& inmap,std::string idprefix, std::ostream& log) {
        geometry<2>::init(inmap,idprefix,log);
        inmap.getwdefault(idprefix+"_radius",radius,0.5);
        FLT ctr_dflt[2] = {0.0, 0.0};
        inmap.getwdefault(idprefix+"_center",ctr.data(),2,ctr_dflt);
    }
};

class ellipse : public geometry<2> {
public:
    TinyVector<FLT,2> axes;
    FLT hgt(TinyVector<FLT,2>& pt, FLT time = 0.0) {
        return(1 -pow(pt[0]/axes(0),2) -pow(pt[1]/axes(1),2));
    }
    FLT dhgt(int dir, TinyVector<FLT,2>& pt, FLT time = 0.0) {
        return(-2.*pt[dir]/pow(axes(dir),2));
    }
    
public:
    ellipse() : geometry<2>() {}
    ellipse(const ellipse &inbdry) : geometry<2>(inbdry), axes(inbdry.axes) {}
    ellipse* create() const {return(new ellipse(*this));}
    
    void init(input_map& inmap, std::string idprefix,std::ostream& log) {
        geometry<2>::init(inmap,idprefix,log);
        inmap.getwdefault(idprefix+"_a",axes(0),1.0);
        inmap.getwdefault(idprefix+"_b",axes(1),1.0);
    }
};


class naca : public geometry<2> {
public:
    FLT sign;
    TinyVector<FLT,5> coeff;
    FLT scale;
    
    FLT hgt(TinyVector<FLT,2>& pt, FLT time = 0.0) {
        pt *= scale;
        FLT poly = coeff[1]*pt[0] +coeff[2]*pow(pt[0],2) +coeff[3]*pow(pt[0],3) +coeff[4]*pow(pt[0],4) - sign*pt[1];
        return(coeff[0]*pt[0] -poly*poly/coeff[0]);
    }
    FLT dhgt(int dir, TinyVector<FLT,2>& pt, FLT time = 0.0) {
        pt *= scale;
        
        TinyVector<FLT,2> ddx;
        FLT poly = coeff[1]*pt[0] +coeff[2]*pow(pt[0],2) +coeff[3]*pow(pt[0],3) +coeff[4]*pow(pt[0],4) - sign*pt[1];
        FLT dpolydx = coeff[1] +2*coeff[2]*pt[0] +3*coeff[3]*pow(pt[0],2) +4*coeff[4]*pow(pt[0],3);
        FLT dpolydy = -sign;
        ddx(0) = coeff[0] -2*poly*dpolydx/coeff[0];
        ddx(1) = -2*poly*dpolydy/coeff[0];
        ddx *= scale;
        
        if (dir == 0) return(ddx(0));
        return(ddx(1));
    }
    
    naca() : geometry<2>(), sign(1.0), scale(1.0) {
        /* NACA 0012 is the default */
        sign = 1;
        coeff[0] = 1.4845; coeff[1] = -0.63; coeff[2] = -1.758; coeff[3] = 1.4215; coeff[4] = -0.5180;
        coeff *= 0.12;
        // at peak y =0.4950625*0.12
    }
    naca(const naca &inbdry) : geometry<2>(inbdry), sign(inbdry.sign), scale(inbdry.scale) {
        for(int i=0;i<5;++i)
            coeff[i] = inbdry.coeff[i];
    }
    naca* create() const {return(new naca(*this));}
    
    void init(input_map& inmap,std::string idprefix,std::ostream& log) {
        geometry<2>::init(inmap,idprefix,log);
        
        FLT thickness;
        inmap.getwdefault(idprefix+"_sign",sign,1.0);
        inmap.getwdefault(idprefix+"_thickness",thickness,0.12);
        inmap.getwdefault(idprefix+"_scale",scale,1.0);
        scale = 1./scale;
        
        std::string linebuf;
        istringstream datastream;
        FLT naca_0012_dflt[5] = {1.4845, -0.63, -1.758, 1.4215, -0.5180};
        inmap.getwdefault(idprefix+"_coeff",coeff.data(),5,naca_0012_dflt);
        coeff *= thickness;
        
    }
};

class plane : public geometry<2> {
public:
    // Rotation and position information is kept in rigid class
    FLT hgt(TinyVector<FLT,2>& xin, FLT time = 0.0) {
        return(xin[0]);
    }
    FLT dhgt(int dir, TinyVector<FLT,2>& x, FLT time = 0.0) {
        if (dir == 0) return(1.0);
        return(0.0);
    }
    
    plane() :geometry<2>() {}
    plane(const plane &inbdry) : geometry<2>(inbdry) {}
    plane* create() const {return(new plane(*this));}
};

#include <spline.h>

class spline_geometry {
    static const int ND = 2;
protected:
    spline3<ND> my_spline;
    FLT smin, smax; // LIMITS FOR BOUNDARY
    FLT scale;
public:
    void init(input_map& inmap,std::string idprefix,std::ostream& log) {
        std::string line;
        if (!inmap.get(idprefix+"_filename",line)) {
            log << "Couldn't fine spline file name in input file\n";
        }
        my_spline.read(line);
        
        inmap.getlinewdefault(idprefix+"_s_limits",line,"0 1");
        std::istringstream data(line);
        data >> smin >> smax;
        data.clear();
        
        inmap.getwdefault(idprefix+"_scale",scale,1.0);
    }
    
    int mvpttobdry(TinyVector<FLT,ND> &pt, FLT time) {
        FLT sloc;
        
        pt /= scale;
        my_spline.find(sloc,pt);
        if (sloc > smax) sloc = smax;
        if (sloc < smin) sloc = smin;
        my_spline.interpolate(sloc,pt);
        pt *= scale;
        
        return(0);
    }
    void bdry_normal(TinyVector<FLT,ND> pt, FLT time, TinyVector<FLT,ND>& norm) {
        std::cerr << "bdry_normal not implemented for spline_geoemtry" << std::endl;
        return;
    }
};
#endif


