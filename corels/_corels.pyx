# distutils: language = c++
# cython: language_level = 3

from libc.string cimport strdup, strcpy
from libc.stdlib cimport malloc, free
from libcpp.vector cimport vector
from libcpp.set cimport set
from libcpp.string cimport string
import numpy as np
cimport numpy as np
cimport cython

cdef extern from "src/corels/src/rule.h":
    ctypedef unsigned long* VECTOR
    cdef struct rule:
        VECTOR truthtable
        char* features
        int cardinality
        int* ids
        int support

    ctypedef rule rule_t
    
    int ascii_to_vector(char *, size_t, int *, int *, VECTOR *)
    void rules_free(rule_t *, const int, int);
    int rule_vfree(VECTOR *)
    int rule_vinit(int, VECTOR *)
    void rule_not(VECTOR, VECTOR, int, int *)
    int rule_isset(VECTOR, int)
    int count_ones_vector(VECTOR, int)

cdef extern from "src/corels/src/run.h":
    int run_corels_begin(double c, char* vstring, int curiosity_policy,
                      int map_type, int ablation, int calculate_size, int nrules, int nlabels,
                      int nsamples, rule_t* rules, rule_t* labels, rule_t* meta, int freq, char* log_fname,
                      PermutationMap*& pmap, CacheTree*& tree, Queue*& queue, Noise*& noise, double epsilon, double delta, double sensitivity, unsigned int max_length, unsigned int seed, string method, double& init,
                      set[string]& verbosity)

    int run_corels_loop(size_t max_num_nodes, PermutationMap* pmap, CacheTree* tree, Queue* queue, Noise* noise, unsigned int max_length)

    double run_corels_end(vector[int]* rulelist, vector[int]* classes, int early, int latex_out, rule_t* rules,
                          rule_t* labels, char* opt_fname, PermutationMap*& pmap, CacheTree*& tree, Queue*& queue, Noise*& noise,
                          double init, set[string]& verbosity)

cdef extern from "src/utils.hh":
    int mine_rules(char **features, rule_t *samples, int nfeatures, int nsamples, 
                int max_card, double min_support, rule_t **rules_out, int verbose)

    int minority(rule_t* rules, int nrules, rule_t* labels, int nsamples, rule_t* minority_out, int verbose)

cdef extern from "src/corels/src/pmap.h":
    cdef cppclass PermutationMap:
        pass

cdef extern from "src/corels/src/cache.h":
    cdef cppclass CacheTree:
        pass

cdef extern from "src/corels/src/queue.h":
    cdef cppclass Queue:
        pass

cdef extern from "src/corels/src/noise.h":
    cdef cppclass Noise:
        pass
        
@cython.boundscheck(False)
@cython.wraparound(False)
def predict_wrap(np.ndarray[np.uint8_t, ndim=2] X, rules):
    cdef int nsamples = X.shape[0]
    cdef int nfeatures = X.shape[1]
    
    cdef np.ndarray out = np.zeros(nsamples, dtype=np.uint8)
    cdef int n_rules = len(rules) - 1
    if n_rules < 0:
        return out

    cdef int s, r, next_rule, nidx, a, idx, c
    cdef int default = bool(rules[n_rules]["prediction"])

    cdef int* antecedent_lengths = <int*>malloc(sizeof(int) * n_rules)
    cdef int* predictions = <int*>malloc(sizeof(int) * n_rules)
    cdef int** antecedents = <int**>malloc(sizeof(int*) * n_rules)
    
    for r in range(n_rules):
        antecedent_lengths[r] = len(rules[r]["antecedents"])
        predictions[r] = int(rules[r]["prediction"])
        antecedents[r] = <int*>malloc(sizeof(int) * antecedent_lengths[r])
        for a in range(antecedent_lengths[r]):
            antecedents[r][a] = rules[r]["antecedents"][a]

    # This compiles to C, so it's pretty fast!
    for s in range(nsamples):
        next_rule = 1
        for r in range(n_rules):
            next_rule = 0
            nidx = antecedent_lengths[r]
            for a in range(nidx): #Go through all the antecedents of a given rule r
                idx = antecedents[r][a] 
                c = 1
                if idx < 0: 	#whether it is the antecedent or its negation
                    idx = -idx
                    c = 0

                idx = idx - 1 	# I suppose that a shift (of 1) was made because otherwise if idx = 0 we don't know if it's the negation or not.
                		    #Consider the case where we consider the feature 0. The input is idx =1, it is not reversed because > 0. 
                		    #Then substract 1, it gives idx = 0 corresponding to feature 0.
                		    #Had we wanted to have the negation of feature 0. The input is idx =-1, it is reversed because < 0. 
                		    #Then substract 1, it gives idx = 0 corresponding to feature 0.
                		
                if idx >= nfeatures or X[s, idx] != c: #What does the first condition mean? If the antecedent is invalid? It is most probably a
                				          #guard. Second cond is if the antecedent is not true for sample then don't catch the sample.
                				          #Try with the next rule.	
                   			               
                    next_rule = 1
                    break

            if next_rule == 0: #the sample is captured by the current rule (i.e. all antecedents of the rule are true for the sample) 
            			   #so no next rule is assessed
                out[s] = predictions[r];
                break

        if next_rule == 1: #default rule (the sample was not capture by any of the previous rules)
            out[s] = default

    for r in range(n_rules):
        free(antecedents[r])
    free(antecedents)
    free(predictions)
    free(antecedent_lengths)

    return out

cdef rule_t* _to_vector(np.ndarray[np.uint8_t, ndim=2] X, int* ncount_out):
    d0 = X.shape[0]
    d1 = X.shape[1]
    cdef rule_t* vectors = <rule_t*>malloc(d0 * sizeof(rule_t))
    if vectors == NULL:
        raise MemoryError()

    cdef int nones, ncount;

    for i in range(d0):
        arrstr = ""
        for j in range(d1):
            if X[i][j]:
                arrstr += "1"
            else:
                arrstr += "0"
        
        bytestr = arrstr.encode("ascii")
        ncount = len(bytestr)
        if ascii_to_vector(bytestr, ncount, &ncount, &nones, &vectors[i].truthtable) != 0:
            for j in range(i):
                rule_vfree(&vectors[j].truthtable)

            free(vectors)
            raise ValueError("Could not load samples")

        ncount_out[0] = ncount

        vectors[i].ids = NULL
        vectors[i].features = NULL
        vectors[i].cardinality = 1
        vectors[i].support = nones

    return vectors

cdef _free_vector(rule_t* vs, int count):
    if vs == NULL:
        return
    
    for i in range(count):
        rule_vfree(&vs[i].truthtable)
        if vs[i].ids:
            free(vs[i].ids)

        if vs[i].features:
            free(vs[i].features)
    
    free(vs)

cdef rule_t* rules = NULL
cdef rule_t* labels_vecs = NULL
cdef rule_t* minor = NULL
cdef int n_rules = 0
cdef PermutationMap* pmap = NULL
cdef CacheTree* tree = NULL
cdef Queue* queue = NULL
cdef Noise* noise = NULL
cdef double init = 0.0
cdef set[string] run_verbosity


max_length_global = 0

def fit_wrap_begin(np.ndarray[np.uint8_t, ndim=2] samples, 
             np.ndarray[np.uint8_t, ndim=2] labels,
             features, int max_card, double min_support, 
             double epsilon, double delta, double global_sens, unsigned int max_length, unsigned int seed,  string method,
             verbosity_str, int mine_verbose,
             int minor_verbose, double c, int policy, int map_type, int ablation,
             int calculate_size):
    global rules
    global labels_vecs
    global minor
    global n_rules
    global noise
    global max_length_global
    max_length_global = max_length

    cdef int nfeatures = 0
    cdef rule_t* samples_vecs = _to_vector(samples, &nfeatures)

    nsamples = samples.shape[0]
    
    #noise = new Noise(epsilon, delta, global_sens, max_length, nsamples, seed, method)

    if nfeatures > len(features):
        if samples_vecs != NULL:
            _free_vector(samples_vecs, nsamples)
            samples_vecs = NULL
        raise ValueError("Feature count mismatch between sample data (" + str(nfeatures) + 
                         ") and feature names (" + str(len(features)) + ")")

    cdef char** features_vec = <char**>malloc(nfeatures * sizeof(char*))
    if features_vec == NULL:
        if samples_vecs != NULL:
            _free_vector(samples_vecs, nsamples)
            samples_vecs = NULL
        raise MemoryError()

    for i in range(nfeatures):
        bytestr = features[i].encode("ascii")
        features_vec[i] = strdup(bytestr)
        if features_vec[i] == NULL:
            for j in range(i):
                if features_vec[j] != NULL:
                    free(features_vec[j])
            features_vec = NULL
            if samples_vecs != NULL:
                _free_vector(samples_vecs, nsamples)
                samples_vecs = NULL
            raise MemoryError()

    if rules != NULL:
        _free_vector(rules, n_rules)
        rules = NULL
    n_rules = 0

    cdef int r = mine_rules(features_vec, samples_vecs, nfeatures, nsamples,
                max_card, min_support, &rules, mine_verbose)

    if features_vec != NULL:
        for i in range(nfeatures):
            if features_vec[i] != NULL:
                free(features_vec[i])
        free(features_vec)
        features_vec = NULL
   
    if samples_vecs != NULL:
        _free_vector(samples_vecs, nsamples)
        samples_vecs = NULL

    if r == -1 or rules == NULL:
        raise MemoryError();
    
    n_rules = r

    verbosity_ascii = verbosity_str.encode("ascii")
    cdef char* verbosity = verbosity_ascii

    if labels_vecs != NULL:
        _free_vector(labels_vecs, 2)
        labels_vecs = NULL

    cdef int nsamples_chk = 0
    try:
        labels_vecs = _to_vector(labels, &nsamples_chk)
    except:
        if rules != NULL:
            _free_vector(rules, n_rules)
            rules = NULL
        n_rules = 0
        raise

    if nsamples_chk != nsamples:
        if labels_vecs != NULL:
            _free_vector(labels_vecs, 2)
            labels_vecs = NULL
        if rules != NULL:
            _free_vector(rules, n_rules)
            rules = NULL
        n_rules = 0
        raise ValueError("Sample count mismatch between label (" + str(nsamples_chk) +
                         ") and rule data (" + str(nsamples) + ")")

    labels_vecs[0].features = <char*>malloc(8)
    labels_vecs[1].features = <char*>malloc(8)
    if labels_vecs[0].features == NULL or labels_vecs[1].features == NULL:
        if labels_vecs != NULL:
            _free_vector(labels_vecs, 2)
            labels_vecs = NULL
        if rules != NULL:
            _free_vector(rules, n_rules)
            rules = NULL
        n_rules = 0
        raise MemoryError();
    strcpy(labels_vecs[0].features, "label=0")
    strcpy(labels_vecs[1].features, "label=1")
    
    if minor != NULL:
        _free_vector(minor, 1)
        minor = NULL

    minor = <rule_t*>malloc(sizeof(rule_t))
    if minor == NULL:
        if labels_vecs != NULL:
            _free_vector(labels_vecs, 2)
            labels_vecs = NULL
        if rules != NULL:
            _free_vector(rules, n_rules)
            rules = NULL
        n_rules = 0
        raise MemoryError();

    cdef int mr = minority(rules, n_rules, labels_vecs, nsamples, minor, minor_verbose)
    if mr != 0:
        if labels_vecs != NULL:
            _free_vector(labels_vecs, 2)
            labels_vecs = NULL
        if rules != NULL:
            _free_vector(rules, n_rules)
            rules = NULL
        n_rules = 0
        raise MemoryError()
    """    
    if count_ones_vector(minor[0].truthtable, nsamples) <= 0:
        if minor != NULL:
            _free_vector(minor, 1)
            minor = NULL
    """
    
    cdef int rb = run_corels_begin(c, verbosity, policy, map_type, ablation, calculate_size,
                   n_rules, 2, nsamples, rules, labels_vecs, minor, 0, NULL, pmap, tree,
                   queue, noise, epsilon, delta, global_sens, max_length, seed, method, init, run_verbosity)

    if rb == -1:
        if labels_vecs != NULL:
            _free_vector(labels_vecs, 2)
            labels_vecs = NULL
        if minor != NULL:
            _free_vector(minor, 1)
            minor = NULL
        if rules != NULL:
            _free_vector(rules, n_rules)
            rules = NULL
        n_rules = 0

        return False

    return True

def fit_wrap_loop(size_t max_nodes):
    global noise
    global max_length_global
    cdef size_t max_num_nodes = max_nodes
    # This is where the magic happens
    return (run_corels_loop(max_num_nodes, pmap, tree, queue, noise, max_length_global) != -1)

def fit_wrap_end(int early):
    global rules
    global labels_vecs
    global minor
    global n_rules

    cdef vector[int] rulelist
    cdef vector[int] classes
    run_corels_end(&rulelist, &classes, early, 0, NULL, NULL, NULL, pmap, tree,
                    queue, noise, init, run_verbosity)

    r_out = []
    print(rulelist.size())
    for i in range(rulelist.size()):
        if rulelist[i] < n_rules:
            r_out.append({})
            r_out[i]["antecedents"] = []
            for j in range(rules[rulelist[i]].cardinality):
                r_out[i]["antecedents"].append(rules[rulelist[i]].ids[j])

            r_out[i]["prediction"] = bool(classes[i])

    r_out.append({ "antecedents": [0], "prediction": bool(classes[rulelist.size()]) })

    # Exiting early skips cleanup
    if early == 0:   
        if labels_vecs != NULL: 
            _free_vector(labels_vecs, 2)
        if minor != NULL: 
            _free_vector(minor, 1)
        if rules != NULL: 
            _free_vector(rules, n_rules)
    
    minor = NULL
    rules = NULL
    labels_vecs = NULL
    n_rules = 0

    return r_out
