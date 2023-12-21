#include "queue.h"
#include "noise.h"

#include <algorithm>
#include <iostream>
#include <stdio.h>

#define NO_BOUNDS

#if defined(R_BUILD)
 #define STRICT_R_HEADERS
 #include "R.h"
 // textual substitution
 #define printf Rprintf
#endif

Queue::Queue(std::function<bool(Node*, Node*)> cmp, char const *type)
    : q_(new q (cmp)), type_(type) {}

Queue::~Queue() {
    if(q_)
        delete q_;
}

/*
 * Performs incremental computation on a node, evaluating the bounds and inserting into the cache,
 * queue, and permutation map if appropriate.
 * This is the function that contains the majority of the logic of the algorithm.
 *
 * parent -- the node that is going to have all of its children evaluated.
 * parent_not_captured -- the vector representing data points NOT captured by the parent.
 */
void evaluate_children(CacheTree* tree, Node* parent, tracking_vector<unsigned short, DataStruct::Tree> parent_prefix,
        VECTOR parent_not_captured, Queue* q, PermutationMap* p, Noise *noise) {
    VECTOR captured, captured_zeros, not_captured, not_captured_zeros, not_captured_equivalent;
    int num_captured, c0, c1, captured_correct;
    int num_not_captured, d0, d1, default_correct, num_not_captured_equivalent;
    bool prediction, default_prediction;
    double lower_bound, objective, parent_lower_bound, lookahead_bound;
    double parent_equivalent_minority;
    double equivalent_minority = 0.;
    int nsamples = tree->nsamples();
    int nrules = tree->nrules();
    double c = tree->c();
    double threshold = c * nsamples; //For bound on antecedent support : n_v < N*c
    bool no_bounds = false; //when removing bounds for DP algo
    rule_vinit(nsamples, &captured);
    rule_vinit(nsamples, &captured_zeros);
    rule_vinit(nsamples, &not_captured);
    rule_vinit(nsamples, &not_captured_zeros);
    rule_vinit(nsamples, &not_captured_equivalent);
    //For debug : to ensure that the generator is not reset at each iteration
    //std::cout << "Laplace noise test " << noise->laplace_noise() << std::endl;
    int i, len_prefix;
    len_prefix = parent->depth() + 1;
    parent_lower_bound = parent->lower_bound(); //b + b0
    parent_equivalent_minority = parent->equivalent_minority(); //b0
    std::set<std::string> verbosity = logger->getVerbosity();
    double t0 = timestamp();
    for (i = 1; i < nrules; i++) {
        double t1 = timestamp();
        // check if this rule is already in the prefix
        if (std::find(parent_prefix.begin(), parent_prefix.end(), i) != parent_prefix.end())
            continue;
        // captured represents data captured by the new rule
        rule_vand(captured, parent_not_captured, tree->rule(i).truthtable, nsamples, &num_captured);

        // lower bound on antecedent support (theorem 10)
        #ifndef NO_BOUNDS
        if ((tree->ablation() != 1) && (num_captured < threshold))
            continue;
        #endif

        rule_vand(captured_zeros, captured, tree->label(0).truthtable, nsamples, &c0); /* c0 : count of data captured with label 0 */
        c1 = num_captured - c0; // c1 : count of data captured with label 1

        // Choose for the rule whether q_i = 1 or 0 (majority label of captured data)
        if (c0 > c1) {
            prediction = 0;
            captured_correct = c0;
        } else {
            prediction = 1;
            captured_correct = c1;
        }

        // lower bound on accurate antecedent support (theorem 11) (denoted n_c in the paper)
        #ifndef NO_BOUNDS
        if ((tree->ablation() != 1) && (captured_correct < threshold))
            continue;
        #endif

        // subtract off parent equivalent points bound because we want to use pure lower bound from parent
        /*b(Dp, x,y) = b(dp, x,y)  + delta (= misclassification ratio error of the new rule) + c (incremental lower bound : see equation 30)
        Actually, the computation below does not take into account this formula, it substracts b0(dp,x,y) yielding : b'(Dp, x,y) = b(dp, x,y) -b0(dp,x,y)  + delta (= misclassification ratio error of the new rule) + c  where b0 formula is reminded below
        Why do this ?
        The variable name lower bound is misleading. We are not computing the quantity b(Dp,x,y) but an analog quantity b'(Dp, x,y) denoted as the pure lower bound (not taking into account insconsistencies of the dataset). See note in cache.h about stored attributes. This is also why we substract b0(dp,x,y) because the incremental formula is for pure lower bounds and the stored values contains the inconsistencies for the parent bound too */
        lower_bound = parent_lower_bound - parent_equivalent_minority + (double)(num_captured - captured_correct) / nsamples + c;
        logger->addToLowerBoundTime(time_diff(t1));
        logger->incLowerBoundNum();

        #ifndef NO_BOUNDS
        if (lower_bound >= tree->min_objective()) /*Hierarchical objective (pure) lower bound (theorem 1) : b(Dp,x,y) >= R^c ==> stop */
	        continue;
        #endif

        double t2 = timestamp();
        rule_vandnot(not_captured, parent_not_captured, captured, nsamples, &num_not_captured);
        rule_vand(not_captured_zeros, not_captured, tree->label(0).truthtable, nsamples, &d0);
        //TODO : implement differentially private mechanism on choice of the default prediction
        d1 = num_not_captured - d0;
        if (d0 > d1) {
            default_prediction = 0;
            default_correct = d0;
        } else {
            default_prediction = 1;
            default_correct = d1;
        }
        /* Incremental computation of objective function using pure lower bound : see(32) */
        objective = lower_bound + (double)(num_not_captured - default_correct) / nsamples;
        logger->addToObjTime(time_diff(t2));
        logger->incObjNum();

        double current_rule_noise = noise->laplace_noise();
        // update current best rule list and objective

        if (objective + current_rule_noise< tree->min_objective() + tree->associated_noise()) {
            if (verbosity.count("progress")) {
                printf("min(objective): %1.5f -> %1.5f, length: %d, cache size: %zu\n",
                   tree->min_objective(), objective, len_prefix, tree->num_nodes());
            }

            logger->setTreeMinObj(objective);
            tree->update_min_objective(objective);
            tree->update_associated_noise(current_rule_noise);
            tree->update_opt_rulelist(parent_prefix, i);
            tree->update_opt_predictions(parent, prediction, default_prediction);
            // dump state when min objective is updated
            logger->dumpState();
        }
        // calculate equivalent points bound to capture the fact that the minority points can never be captured correctly
        /*From the paper : equivalent points bound theorem (20). The data can be separated into equivalent sets (i.e. sets of points captured by the same rule). For a given equivalent set e_u (hence a given rule antecedent p and q_u the minority class inside the equivalent the set),  equivalent minority = b0(Dp,x,y) = |{elements not captured by Dp} AND {elements belonging to the minority class of their equivalent set} |
         Note how the equivalent sets can be computed beforehand (and their minority class) beforehand granted that we know the set of all antecedents*/
        if (tree->has_minority()) {
            rule_vand(not_captured_equivalent, not_captured, tree->minority(0).truthtable, nsamples, &num_not_captured_equivalent);
            equivalent_minority = (double)(num_not_captured_equivalent) / nsamples;
            lower_bound += equivalent_minority; // add b0 to the pure lower bound to compute the true lower bound
        }
        if (tree->ablation() != 2)
            lookahead_bound = lower_bound + c;
        else
            lookahead_bound = lower_bound;

        /* only add node to our datastructures if its children will be viable
         Lookahead bound (lemma 2) */
        #ifdef NO_BOUNDS
        no_bounds = true;
        #endif

        if (no_bounds || lookahead_bound < tree->min_objective()) {
            double t3 = timestamp();
            // check permutation bound
            //TODO : deactivate permutation bound : use -p 0 when launching
            Node* n = p->insert(i, nrules, prediction, default_prediction,
                                   lower_bound, objective, parent, num_not_captured, nsamples,
                                   len_prefix, c, equivalent_minority, tree, not_captured, parent_prefix);
            logger->addToPermMapInsertionTime(time_diff(t3));
            // n is NULL if this rule fails the permutation bound
            if (n) {
                double t4 = timestamp();
                tree->insert(n);
                logger->incTreeInsertionNum();
                logger->incPrefixLen(len_prefix);
                logger->addToTreeInsertionTime(time_diff(t4));
                double t5 = timestamp();
                q->push(n);
                logger->setQueueSize(q->size());
                if (tree->calculate_size())
                    logger->addQueueElement(len_prefix, lower_bound, false);
                logger->addToQueueInsertionTime(time_diff(t5));
            }
        } // else:  objective lower bound with one-step lookahead
    }

    rule_vfree(&captured);
    rule_vfree(&captured_zeros);
    rule_vfree(&not_captured);
    rule_vfree(&not_captured_zeros);
    rule_vfree(&not_captured_equivalent);

    logger->addToRuleEvalTime(time_diff(t0));
    logger->incRuleEvalNum();
    logger->decPrefixLen(parent->depth());
    if (tree->calculate_size())
        logger->removeQueueElement(len_prefix - 1, parent_lower_bound, false);
    if (parent->num_children() == 0) {
        tree->prune_up(parent);
    } else {
        parent->set_done();
        tree->increment_num_evaluated();
    }
}

static size_t num_iter = 0;
static double min_objective = 0.0;
static VECTOR captured, not_captured;
static double start = 0.0;

/*
 * Explores the search space by using a queue to order the search process.
 * The queue can be ordered by DFS, BFS, or an alternative priority metric (e.g. lower bound).
 */
void bbound_begin(CacheTree* tree, Queue* q) {
    start = timestamp();
    num_iter = 0;
    rule_vinit(tree->nsamples(), &captured);
    rule_vinit(tree->nsamples(), &not_captured);

    logger->setInitialTime(start);
    logger->initializeState(tree->calculate_size());
    // initial log record
    logger->dumpState();

    min_objective = 1.0;
    tree->insert_root();
    logger->incTreeInsertionNum();
    q->push(tree->root());
    logger->setQueueSize(q->size());
    logger->incPrefixLen(0);
    // log record for empty rule list
    logger->dumpState();
}

void bbound_loop(CacheTree* tree, Queue* q, PermutationMap* p, Noise* noise) {
    double t0 = timestamp();
    std::set<std::string> verbosity = logger->getVerbosity();
    size_t queue_min_length = logger->getQueueMinLen();

    int cnt;
    std::pair<Node*, tracking_vector<unsigned short, DataStruct::Tree> > node_ordered = q->select(tree, captured);
    logger->addToNodeSelectTime(time_diff(t0));
    logger->incNodeSelectNum();
    if (node_ordered.first) {
        double t1 = timestamp();
        // not_captured = default rule truthtable & ~ captured
        rule_vandnot(not_captured,
                     tree->rule(0).truthtable, captured,
                     tree->nsamples(), &cnt);
        evaluate_children(tree, node_ordered.first, node_ordered.second, not_captured, q, p, noise);
        logger->addToEvalChildrenTime(time_diff(t1));
        logger->incEvalChildrenNum();

        if (tree->min_objective() < min_objective) {
            min_objective = tree->min_objective();
            if (verbosity.count("loud"))
                printf("before garbage_collect. num_nodes: %zu\n", tree->num_nodes());
            logger->dumpState();
            tree->garbage_collect();
            logger->dumpState();
            if (verbosity.count("loud"))
                printf("after garbage_collect. num_nodes: %zu\n", tree->num_nodes());
        }
    }
    logger->setQueueSize(q->size());
    if (queue_min_length < logger->getQueueMinLen()) {
        // garbage collect the permutation map: can be simplified for the case of BFS
        queue_min_length = logger->getQueueMinLen();
        //pmap_garbage_collect(p, queue_min_length);
    }
    ++num_iter;
    if ((num_iter % 10000) == 0) {
        if (verbosity.count("loud"))
            printf("iter: %zu, tree: %zu, queue: %zu, pmap: %zu, time elapsed: %f\n",
                   num_iter, tree->num_nodes(), q->size(), p->size(), time_diff(start));
    }
    if ((num_iter % logger->getFrequency()) == 0) {
        // want ~1000 records for detailed figures|| lookahead_bound < tree->min_objective()
        logger->dumpState();
    }
}

int bbound_end(CacheTree* tree, Queue* q, PermutationMap* p, bool early) {
    std::set<std::string> verbosity = logger->getVerbosity();
    bool print_queue = 0;
    logger->dumpState(); // second last log record (before queue elements deleted)
    if (verbosity.count("loud"))
        printf("iter: %zu, tree: %zu, queue: %zu, pmap: %zu, time elapsed: %f\n",
               num_iter, tree->num_nodes(), q->size(), p->size(), time_diff(start));

    if (!early) {
        if (q->empty()) {
            if (verbosity.count("progress"))
                printf("Exited because queue empty\n");
        }
        else if (verbosity.count("progress"))
            printf("Exited because max number of nodes in the tree was reached\n");
    }

    // Print out queue
    ofstream f;
    if (print_queue) {
        char fname[] = "queue.txt";
        if (verbosity.count("progress")) {
            printf("Writing queue elements to: %s\n", fname);
        }
        f.open(fname, ios::out | ios::trunc);
        f << "lower_bound objective length frac_captured rule_list\n";
    }

    // Exiting early skips cleanup
    if(!early) {
        // Clean up data structures
        if (verbosity.count("progress")) {
            printf("Deleting queue elements and corresponding nodes in the cache,"
                "since they may not be reachable by the tree's destructor\n");
            printf("\nminimum objective: %1.10f\n", tree->min_objective());
        }
        Node* node;
        double min_lower_bound = 1.0;
        double lb;
        size_t num = 0;
        while (!q->empty()) {
            node = q->front();
            q->pop();
            if (node->deleted()) {
                tree->decrement_num_nodes();
                logger->removeFromMemory(sizeof(*node), DataStruct::Tree);
                delete node;
            } else {
                lb = node->lower_bound() + tree->c();
                if (lb < min_lower_bound)
                    min_lower_bound = lb;
                if (print_queue) {
                    std::pair<tracking_vector<unsigned short, DataStruct::Tree>, tracking_vector<bool, DataStruct::Tree> > pp_pair = node->get_prefix_and_predictions();
                    tracking_vector<unsigned short, DataStruct::Tree> prefix = std::move(pp_pair.first);
                    tracking_vector<bool, DataStruct::Tree> predictions = std::move(pp_pair.second);
                    f << node->lower_bound() << " " << node->objective() << " " << node->depth() << " "
                      << (double) node->num_captured() / (double) tree->nsamples() << " ";
                    for(size_t i = 0; i < prefix.size(); ++i) {
                        f << tree->rule_features(prefix[i]) << "~"
                          << predictions[i] << ";";
                    }
                    f << "default~" << predictions.back() << "\n";
                    num++;
                }
            }
        }
        if (verbosity.count("progress"))
            printf("minimum lower bound in queue: %1.10f\n\n", min_lower_bound);
    }

    if (print_queue)
        f.close();
    // last log record (before cache deleted)
    logger->dumpState();

    if(!early) {
        rule_vfree(&captured);
        rule_vfree(&not_captured);
    }

    return num_iter;
}
