/*
 * The Citrusleaf C interface. A good, basic library that many clients can be based on.
 *
 * This is the external, public header file
 *
 * this code currently assumes that the server is running in an ASCII-7 based
 * (ie, utf8 or ISO-LATIN-1)
 * character set, as values coming back from the server are UTF-8. We currently
 * don't bother to convert to the character set of the machine we're running on
 * but we advertise these values as 'strings'
 *
 * All rights reserved
 * Brian Bulkowski, 2009
 * CitrusLeaf
 */

#include <inttypes.h>
#include <stdbool.h>
#include <netinet/in.h>
 
// do this both the new skool and old skool way which gives the highest correctness,
// speed, and compatibility
#pragma once

#ifndef XDS	// Hack for the sake of XDS. XDS includes the main CF libs. 
		// We do not want to include them again from client API
#include "citrusleaf/cf_atomic.h"
#include "citrusleaf/cf_ll.h"
#include "citrusleaf/cf_clock.h"
#include "citrusleaf/cf_vector.h"
#include "citrusleaf/cf_queue.h"
#include "citrusleaf/cf_alloc.h"
#include "citrusleaf/cf_digest.h"
#include "citrusleaf/cf_shash.h"
#endif

#define STACK_BUF_SZ (1024 * 16) // provide a safe number for your system - linux tends to have 8M stacks these days
#define DEFAULT_PROGRESS_TIMEOUT 50

#define NODE_NAME_SIZE 20	

#define BINNAME_SIZE	16

#ifdef __cplusplus
extern "C" {
#endif

//
// Return values for the citrusleaf get and put calls
// 

typedef enum cl_rv {
    CITRUSLEAF_FAIL_ASYNCQ_FULL = -3,
    CITRUSLEAF_FAIL_TIMEOUT = -2,
	CITRUSLEAF_FAIL_CLIENT = -1, 	// an out of memory or similar locally
	CITRUSLEAF_OK = 0,
	CITRUSLEAF_FAIL_UNKNOWN = 1, 	// unknown failure on the server side
	CITRUSLEAF_FAIL_NOTFOUND = 2,	//
	CITRUSLEAF_FAIL_GENERATION = 3,  // likely a CAS write, and the write failed
	CITRUSLEAF_FAIL_PARAMETER = 4,   // you passed in bad parameters
	CITRUSLEAF_FAIL_KEY_EXISTS = 5,
	CITRUSLEAF_FAIL_BIN_EXISTS = 6,
	CITRUSLEAF_FAIL_CLUSTER_KEY_MISMATCH = 7,
	CITRUSLEAF_FAIL_PARTITION_OUT_OF_SPACE = 8,
	CITRUSLEAF_FAIL_SERVERSIDE_TIMEOUT = 9,
	CITRUSLEAF_FAIL_NOXDS = 10,
	CITRUSLEAF_FAIL_UNAVAILABLE = 11,
	CITRUSLEAF_FAIL_INCOMPATIBLE_TYPE = 12  // specified operation cannot be performed on that data type
} cl_rv;


// hidden forward reference
typedef struct cl_conn_s cl_conn;

enum cl_type { CL_NULL = 0x00, CL_INT = 0x01, CL_FLOAT = 2, CL_STR = 0x03, CL_BLOB = 0x04,
	CL_TIMESTAMP = 5, CL_DIGEST = 6, CL_JAVA_BLOB = 7, CL_CSHARP_BLOB = 8, CL_PYTHON_BLOB = 9, 
	CL_RUBY_BLOB = 10, CL_PHP_BLOB = 11, CL_ERLANG_BLOB=12, CL_APPEND = 13, CL_LUA_BLOB = 14, 
	CL_JSON_BLOB = 15, CL_UNKNOWN = 666666};

typedef enum cl_type cl_type;

enum cl_write_policy { CL_WRITE_ASYNC, CL_WRITE_ONESHOT, CL_WRITE_RETRY, CL_WRITE_ASSURED };

typedef enum cl_write_policy cl_write_policy;

enum cl_scan_priority { CL_SCAN_PRIORITY_AUTO, CL_SCAN_PRIORITY_LOW, CL_SCAN_PRIORITY_MEDIUM, CL_SCAN_PRIORITY_HIGH };

typedef enum cl_scan_priority cl_scan_priority;


//
// An object is the value in a bin, or it is used as a key
// The object is typed according to the citrusleaf typing system
// These are often stack allocated, and are assigned using the 'wrap' calls
//

typedef struct cl_object_s {
	enum cl_type    type;
	size_t			sz; 
	union {
		char 		*str; // note for str: sz is strlen (not strlen+1 
		void 		*blob;
		int64_t		i64;   // easiest to have one large int type
	} u;

	void *free; // if this is set, this must be freed on destructuion	

} cl_object;


typedef enum cl_operator_type { CL_OP_WRITE, CL_OP_READ, CL_OP_INCR, CL_OP_MC_INCR , CL_OP_PREPEND, CL_OP_APPEND, CL_OP_MC_PREPEND, CL_OP_MC_APPEND, CL_OP_TOUCH, CL_OP_MC_TOUCH} cl_operator;

// A bin is the bin name, and the value set or gotten

typedef struct cl_bin_s {
	char		bin_name[BINNAME_SIZE];
	cl_object	object;
} cl_bin;

// An operation is the bin, plus the operator (write, read, add, etc)
// This structure is used for the more complex 'operate' call,
// which can specify simultaneous operations on multiple bins

typedef struct cl_operation_s {
	cl_bin                bin;
	enum cl_operator_type op;
}cl_operation;
	
// Structure to map the internal address to the external address
typedef struct cl_addrmap {
	char *orig;
	char *alt;
} cl_addrmap;

// 
// Query related structures:
#define MAX_SINDEX_NAME_SIZE	128
// indicate metadata needed to create a secondary index
typedef struct sindex_metadata_t {
	char	iname[MAX_SINDEX_NAME_SIZE];
    char    binname[32];
    char    type[32];
    uint8_t   isuniq;
    uint8_t   istime;
} sindex_metadata_t;

// Range indicates start/end condition for the columns of the indexes.
// Example1: (index on "last_activity" bin) where last_activity < start_time and last_activity > end_time
// Example2: (index on "last_activity" bin for equality) where last_activity == start_time
// Example3: (compound index on "last_activity","state","age") 
//    where last_activity < start_time and last_activity > end_time
//    and state in ["ca","wa","or"]
// 	  and age == 28
typedef struct cl_query_range {
	char		bin_name[BINNAME_SIZE];
	cl_object	start_obj;
	cl_object	end_obj;
} cl_query_range;

typedef enum cl_query_filter_op { CL_FLTR_EQ, CL_FLTR_LT, CL_FLTR_GT, CL_FLTR_LE, CL_FLTR_GE, CL_FLTR_NE, CL_FLTR_EXISTS} cl_query_filter_op;

// Filter indicate a series of post look-up condition in an equivalent "where" clause
// applied to bins other than the indexed bins
// Example1: where 
// Q: how do we nest filters? don't care?
typedef struct cl_query_filter {
	char		bin_name[BINNAME_SIZE];
	cl_object	compare_obj;
	cl_query_filter_op	ftype;
} cl_query_filter;

typedef enum cl_query_orderby_op { CL_ORDERBY_ASC, CL_ORDERBY_DESC} cl_query_orderby_op;

// Order-by indicate a post look-up result ordering
typedef struct cl_query_orderby {
	char		bin_name[BINNAME_SIZE];
	cl_query_orderby_op	ordertype;
} cl_query_orderby;

typedef struct cl_query {
	char		indexname[MAX_SINDEX_NAME_SIZE];
	cf_vector	*binnames;
	cf_vector	*ranges;
	cf_vector	*filters;
	cf_vector	*orderbys;
	int			limit;	
} cl_query;
 
//
// All citrusleaf functions return an integer. This integer is 0 if the
// call has succeeded, and a negative number if it has failed.
// All returns of pointers and objects are done through the parameters.
// (When in C++, use & parameters for return, but we're not there yet)
//
// 'void' return functions are only used for functions that are syntactically
// unable to fail.
//

typedef void (*cl_async_fail_cb) (void *udata, int rv, uint64_t);
typedef void (*cl_async_success_cb) (void *udata, int rv, uint64_t);

//
// Call this init function sometime early, create our mutexes and a few other things.
// We'd prefer if this is only called once
//

int
citrusleaf_init(void);

void
citrusleaf_change_tend_speed(int secs);

int
citrusleaf_async_init(int size_limit, int num_receiver_threads, cl_async_fail_cb fail_cb_fn, cl_async_success_cb success_cb_fn);

int
citrusleaf_async_reinit(int size_limit, unsigned int num_receiver_threads);

void 
citrusleaf_async_getstats(uint64_t *retries, uint64_t *dropouts, int *workitems);

void
citrusleaf_async_set_nw_timeout(int nw_timeout);

//
// If you wish to free up resources used by the citrusleaf client in your process,
// call this - all cl_conn will be invalid, and you'll have to call citrusleaf_init
// again to do anything
//
void citrusleaf_shutdown(void);

void citrusleaf_set_debug(bool debug_flag);

//
// This call will print stats to stderr
//
void citrusleaf_print_stats(void);

//
// cl_object calls
// 

// fill out the object structure with the string in question - no allocs
void citrusleaf_object_init(cl_object *o);
void citrusleaf_object_init_str(cl_object *o, char const *str);
void citrusleaf_object_init_str2(cl_object *o, char const *str, size_t str_len);
void citrusleaf_object_init_blob(cl_object *o, void const *buf, size_t buf_len);
void citrusleaf_object_init_blob2(cl_object *o, void const *buf, size_t buf_len, cl_type type); // several blob types
void citrusleaf_object_init_int(cl_object *o, int64_t i);
void citrusleaf_object_init_null(cl_object *o);
void citrusleaf_object_free(cl_object *o);

// frees all the memory in a bin array that would be returned from get_all but not the bin array itself
void citrusleaf_bins_free(cl_bin *bins, int n_bins);


// use:
// Calling this will return an cl_conn, which is a structure internal to the library.
// You can use this to do multiple calls to Aerospike instead of returning
// and re-getting each time
// although if you have an error, it might be because that particular cluster
// went down and you might need to get a new connection to the cluster.
// (perhaps we'll hide that detail in future APIs)
//
// You will be required to return your connections! That would be called a leak.
//
// the host-port combo should be some way to get to the cluster (often a DNS name)
// However, it might be done. Using the cluster name allows the clustering
// internals to keep track of other host-ports which are the same cluster,
// and match your request to other hosts

// forward, hidden reference
struct cl_cluster_s;
typedef struct cl_cluster_s cl_cluster;

extern cl_cluster *citrusleaf_cluster_create(void);
extern void citrusleaf_cluster_destroy(cl_cluster *asc);
extern void citrusleaf_cluster_shutdown(void);

extern cl_cluster * citrusleaf_cluster_get_or_create(char *host, short port, int timeout_ms);
extern void citrusleaf_cluster_release_or_destroy(cl_cluster **asc);
extern void citrusleaf_cluster_change_tend_speed(struct cl_cluster_s *asc, int secs);
extern void citrusleaf_cluster_use_nbconnect(struct cl_cluster_s *asc);

// the timeout is how long to wait before the cluster is "settled"
// 0 - a sensible default
// N - some number of MS
// -1 - don't wait this time

extern cl_rv citrusleaf_cluster_add_host(cl_cluster *asc, char const *host, short port, int timeout_ms);

extern void  citrusleaf_cluster_add_addr_map(cl_cluster *asc, char *orig, char *alt);

extern bool citrusleaf_cluster_settled(cl_cluster *asc);

extern int citrusleaf_cluster_get_nodecount(cl_cluster *asc);

// must free node_names when done
extern void cl_cluster_get_node_names(cl_cluster *asc, int *n_nodes, char **node_names);


// in the PHP system, URLs are lingua franca. We expect that
// a single cluster will be created with one name - the URL - and 
// will be used over and over.
//
// URLs are of the form;
// citrusleaf://host:port (or similar)
extern cl_cluster *citrusleaf_cluster_get(char const *url);


// By default, the C client will "follow" the cluster, that is,
// track all the nodes in the cluster and continually update the cluster
// members. If, for testing, you wish to disable this feature, set this
// flag to false. This must be done before any 'addhost' calls, because
// even at the first one, the following of the cluster might start.
//
// Currently, setting this flags after hosts has an undefined effect.

extern void citrusleaf_cluster_follow(cl_cluster *asc, bool flag);

//
// write info structure
// There's a lot of info that can go into a write ---
typedef struct {
	bool unique;  // write unique - means success if didn't exist before
	bool unique_bin;  // write unique bin - means success if the bin didn't exist before
	bool use_generation;     // generation must be exact for write to succeed
	bool use_generation_gt;  // generation must be less - good for backup & restore
	bool use_generation_dup;	// on generation collision, create a duplicat
	uint32_t generation;
	int timeout_ms;
	uint32_t record_ttl;    // seconds, from now, when the record would be auto-removed from the DBcd 
	cl_write_policy w_pol;
} cl_write_parameters;

static inline void
cl_write_parameters_set_default(cl_write_parameters *cl_w_p)
{
	cl_w_p->unique = false;
	cl_w_p->unique_bin = false;
	cl_w_p->use_generation = false;
	cl_w_p->use_generation_gt = false;
	cl_w_p->use_generation_dup = false;
	cl_w_p->timeout_ms = 0;
	cl_w_p->record_ttl = 0;
	cl_w_p->w_pol = CL_WRITE_RETRY;
}

static inline void
cl_write_parameters_set_generation( cl_write_parameters *cl_w_p, uint32_t generation) {
	cl_w_p->generation = generation;
	cl_w_p->use_generation = true;
}

static inline void
cl_write_parameters_set_generation_gt( cl_write_parameters *cl_w_p, uint32_t generation) {
	cl_w_p->generation = generation;
	cl_w_p->use_generation_gt = true;
}

static inline void
cl_write_parameters_set_generation_dup( cl_write_parameters *cl_w_p, uint32_t generation) {
	cl_w_p->generation = generation;
	cl_w_p->use_generation_dup = true;
}


// scan_option info
typedef struct cl_scan_parameters_s {
	bool fail_on_cluster_change;	// honored by server: terminate scan if cluster in fluctuating state
	cl_scan_priority	priority;	// honored by server: priority of scan
	bool concurrent_nodes;	  		// honored on client: work on nodes in parallel or serially
	uint8_t	threads_per_node;		// honored on client: have multiple threads per node. @TODO
} cl_scan_parameters;

static inline void
cl_scan_parameters_set_default(cl_scan_parameters *cl_scan_p)
{
	cl_scan_p->fail_on_cluster_change = false;
	cl_scan_p->concurrent_nodes = false;
	cl_scan_p->threads_per_node = 1;	// not honored currently
	cl_scan_p->priority = CL_SCAN_PRIORITY_AUTO;
}

typedef struct cl_node_response_s {
	char	node_name[NODE_NAME_SIZE];
	cl_rv	node_response;
} cl_node_response;
//
// get-all will malloc an array of values and return all current values for a row.
// thus, it is SELECT * in SQL. So pass in a pointer to cl_value to be filled, and a
// pointer-to-int to know how many.
// 
// The memory contract with get_all is that the bins pointer (*bins) must be freed by the caller.
// The data you've actually retrieved (cl_object->u.str ; cl->object->u.blob) has been allocated using malloc.
// You may use it for your own purposes, transfer it to another program, or you must free it.
// citrusleaf_object_free() will free the internal memory in these cases,
// or you can call citrusleaf_bins_free and it will take care of all memory.
//
// Note this is different from getting a specific set of bins, where the bin array was passed in.
// (the simple 'get') See that call for information there.
 

cl_rv
citrusleaf_get_all(cl_cluster *asc, const char *ns, const char *set, const cl_object *key, cl_bin **bins, int *n_bins, int timeout_ms, uint32_t *cl_gen);

cl_rv
citrusleaf_get_all_digest(cl_cluster *asc, const char *ns, const cf_digest *d, cl_bin **bins, int *n_bins, int timeout_ms, uint32_t *cl_gen);

//
// Put is like insert. Create a list of bins, and call this function to set them.
//
	
cl_rv
citrusleaf_put(cl_cluster *asc, const char *ns, const char *set, const cl_object *key, const cl_bin *bins, int n_bins, const cl_write_parameters *cl_w_p);

cl_rv
citrusleaf_put_digest(cl_cluster *asc, const char *ns, const cf_digest *d, const cl_bin *bins, int n_bins, const cl_write_parameters *cl_w_p);

cl_rv
citrusleaf_put_replace(cl_cluster *asc, const char *ns, const char *set, const cl_object *key, const cl_bin *values, int n_values, const cl_write_parameters *cl_w_p);

cl_rv
citrusleaf_restore(cl_cluster *asc, const char *ns, const cf_digest *digest, const char *set, const cl_bin *values, int n_values, const cl_write_parameters *cl_w_p);

//Async versions of the put calls
cl_rv
citrusleaf_async_put(cl_cluster *asc, const char *ns, const char *set, const cl_object *key, const cl_bin *bins, 
						int n_bins, const cl_write_parameters *cl_w_p, uint64_t trid, void *udata);

cl_rv
citrusleaf_async_put_digest(cl_cluster *asc, const char *ns, const cf_digest *d, const cl_bin *bins, int n_bins, 
						const cl_write_parameters *cl_w_p, uint64_t trid, void *udata);


//
// Get is like select in SQL. Create a list of bins to get, and call this function to retrieve
// the values.

cl_rv
citrusleaf_get(cl_cluster *asc, const char *ns, const char *set, const cl_object *key, cl_bin *bins, int n_bins, int timeout_ms, uint32_t *cl_gen);

cl_rv
citrusleaf_get_digest(cl_cluster *asc, const char *ns, const cf_digest *d, cl_bin *bins, int n_bins, int timeout_ms, uint32_t *cl_gen);

//
// Delete simply wipes this single key off the face of the earth.
//

cl_rv
citrusleaf_delete(cl_cluster *asc, const char *ns, const char *set, const cl_object *key, const cl_write_parameters *cl_w_p);

cl_rv
citrusleaf_delete_digest(cl_cluster *asc, const char *ns,  const cf_digest *d, const cl_write_parameters *cl_w_p);

//
// Efficiently determine if the key exists.
//  (Note:  The bins are currently ignored but may be testable in the future.)
//

cl_rv
citrusleaf_exists_key(cl_cluster *asc, const char *ns, const char *set, const cl_object *key, cl_bin *bins, int n_bins, int timeout_ms, uint32_t *cl_gen);

cl_rv
citrusleaf_exists_digest(cl_cluster *asc, const char *ns, const cf_digest *d, cl_bin *bins, int n_bins, int timeout_ms, uint32_t *cl_gen);

//
// get many call has the following properties:
// you can pass null either with the namespace or the set, and a large iteration will occur
// Memory available vanishes after 'return', if you want a copy, make a copy
// Non-zero return in the callback aborts the call
typedef int (*citrusleaf_get_many_cb) (char *ns, cf_digest *keyd, char *set, uint32_t generation, uint32_t record_ttl,
	cl_bin *bins, int n_bins, bool is_last, void *udata);

cl_rv
citrusleaf_scan(cl_cluster *asc, char *ns, char *set, cl_bin *bins, int n_bins, bool get_key, citrusleaf_get_many_cb cb, void *udata, bool nobindata);

// response is a vector of cl_node_response
cf_vector *
citrusleaf_scan_all_nodes (cl_cluster *asc, char *ns, char *set, cl_bin *bins, int n_bins, bool nobindata, uint8_t scan_pct,
		citrusleaf_get_many_cb cb, void *udata, cl_scan_parameters *scan_p);

cl_rv
citrusleaf_scan_node (cl_cluster *asc, char *node_name, char *ns, char *set, cl_bin *bins, int n_bins, bool nobindata, uint8_t scan_pct,
		citrusleaf_get_many_cb cb, void *udata, cl_scan_parameters *scan_p);

//
// Get many digest
// This version of the call acts exactly as the get digest call, but takes an array
// of digests. Those digests will be retrieved with the same API conventions as the
// previous ones.
// To come: an array of keys, but it'll just be a wrapper on this.
//

cl_rv
citrusleaf_get_many_digest(cl_cluster *asc, char *ns, const cf_digest *digests, int n_digests, cl_bin *bins, int n_bins, bool get_key /*if true, retrieve key instead of simply digest*/, 
	citrusleaf_get_many_cb cb, void *udata);

//
// Key exists many digest
// This version of the call acts exactly as the exists digest call, but takes an array
// of digests. Those digests will be retrieved with the same API conventions as the
// previous ones.
// To come: an array of keys, but it'll just be a wrapper on this.
//

cl_rv
citrusleaf_exists_many_digest(cl_cluster *asc, char *ns, const cf_digest *digests, int n_digests, cl_bin *bins, int n_bins, bool get_key /*if true, retrieve key instead of simply digest*/, 
	citrusleaf_get_many_cb cb, void *udata);

//
// Use the information system to request data from a given node
// Pass in a '\n' seperated list of names, or no names at all
// [Perhaps a better interface would be an array of pointers-to-names, returning an
//  array of pointers-to-values?]
// Returns a malloc'd string which is the response from the server.
//

int
citrusleaf_info(char *host, short port, char *names, char **values, int timeout_ms);

int 
citrusleaf_info_host(struct sockaddr_in *sa_in, char *names, char **values, int timeout_ms, bool send_asis);

// Do a lookup with this name and port, and add the sockaddr to the
// vector using the unique lookup
int cl_lookup(cl_cluster *asc, char *hostname, short port, cf_vector *sockaddr_in_v);

//
// This call is good for testing. Call it when you think you know the values. If the key doesn't exist, or
// the data is incorrect, then the server that is serving the request will spit a failure, and if you're
// running in the right server debug mode you can examine the error in detail.
//

cl_rv
citrusleaf_verify(cl_cluster *asc, const char *ns, const char *set, const cl_object *key, const cl_bin *bins, int n_bins, int timeout_ms, uint32_t *cl_gen);
cl_rv
citrusleaf_delete_verify(cl_cluster *asc, const char *ns, const char *set, const cl_object *key, const cl_write_parameters *cl_w_p);

//
// This call allows the caller to specify the operation - read, write, add, etc.  Multiple operations
// can be specified in a single call.//

cl_rv
citrusleaf_operate(cl_cluster *asc, const char *ns, const char *set, const cl_object *key, cl_operation *operations, int n_operations, const cl_write_parameters *cl_w_p, int replace, uint32_t *generation);

//
// This debugging call can be useful for tracking down errors and coordinating with server failures
// gets the digest for a particular set and key
int
citrusleaf_calculate_digest(const char *set, const cl_object *key, cf_digest *digest);

//
// Lua and secondary index functions
//
struct map_args_t;
// DDL To Be obsoleted                                       
cl_rv citrusleaf_register_lua_function(cl_cluster *asc, char *ns,
                                       citrusleaf_get_many_cb cb,
                                       char *lua_mapf, char *lua_rdcf,
                                       char *lua_fnzf, int reg_mrjid);
struct index_metadata_t;
cl_rv citrusleaf_create_secondary_index(cl_cluster *asc, char *ns,
                                        citrusleaf_get_many_cb cb,
                                        struct index_metadata_t *imd);

// New
cl_rv citrusleaf_secondary_index_create(cl_cluster *asc, const char *ns, 
		const char *set, struct sindex_metadata_t *simd);
cl_rv citrusleaf_secondary_index_delete(cl_cluster *asc, const char *ns, 
		const char *set, const char *indexname);


// RANGE QUERIES To Be obsoleted
cl_rv citrusleaf_get_sik_digest(cl_cluster *asc, char *ns,
                                const cf_digest *digests, int n_digests,
                                cl_bin *bins, int n_bins, bool get_key,
                                citrusleaf_get_many_cb cb, void *udata,
                                int imatch);
cl_rv citrusleaf_run_mr_sik_digest(cl_cluster *asc, char *ns,
                                   const cf_digest *digests, int n_digests,
                                   cl_bin *bins, int n_bins, bool get_key,
                                   citrusleaf_get_many_cb cb, void *udata,
                                   int mrjid, int imatch,
                                   struct map_args_t *margs);
// New
cl_query *citrusleaf_query_create(const char *indexname);
void citrusleaf_query_destroy(cl_query *query_obj);
cl_rv citrusleaf_query_add_binname(cl_query *query_obj, const char *binname);
cl_rv citrusleaf_query_add_range_numeric(cl_query *query_obj, const char *binname,int64_t start,int64_t end);
cl_rv citrusleaf_query_add_range_string(cl_query *query_obj, const char *binname, const char *start, const char *end);
cl_rv citrusleaf_query_add_filter_numeric(cl_query *query_obj, const char *binname, int64_t comparer, cl_query_filter_op op);
cl_rv citrusleaf_query_add_filter_string(cl_query *query_obj, const char *binname, const char *comparer, cl_query_filter_op op);
cl_rv citrusleaf_query_add_orderby(cl_query *query_obj, const char *binname, cl_query_orderby_op order);
cl_rv citrusleaf_query_set_limit(cl_query *query_obj, uint64_t limit);

cl_rv citrusleaf_query(cl_cluster *asc, const char *ns, const cl_query *query_obj,
							citrusleaf_get_many_cb cb, void *udata);

#ifdef __cplusplus
} // end extern "C"
#endif


