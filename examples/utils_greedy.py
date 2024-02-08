import numpy as np

def get_rule_str(a_rule, features):
    res = "{"
    for a_one_rule in a_rule:
        if res != "{":
            res = res + ","
        if a_one_rule > 0:
            res = res + features[a_one_rule-1]
        else:
            res = res + features[(-a_one_rule)-1] + "-not"
    res = res + "}"
    return res

def mine_rules_combinations(X, max_card, min_support, allow_negations, features, verbosity):
    if max_card > 1:
        from itertools import combinations # Needed to mine rules with more than a single antecedent

    list_of_rules = []
    tot_rules = 0

    # === Card 1 rules ===
    one_rules = []
    for a_rule in range(1, X.shape[1]+1):
        pos_support = np.mean(X[:,a_rule-1])
        if pos_support >= min_support: # check min support & keep rules for further mining
            one_rules.append(a_rule) # keep for further mining without checking max. support
            if pos_support <= 1 - min_support: # also check max support
                list_of_rules.append([a_rule])
                tot_rules += 1
                if "mine" in verbosity:
                    print("(%d) %s with support %.6f" %(tot_rules, features[a_rule-1], pos_support))
        if allow_negations and (1-pos_support) >= min_support:  # check min support & keep rules for further mining
            one_rules.append(-a_rule) # keep for further mining without checking max. support
            if (1-pos_support) <= 1 - min_support: # also check max support
                list_of_rules.append([-a_rule])
                tot_rules += 1
                if "mine" in verbosity:
                    print("(%d) %s-not with support %.6f" %(tot_rules, features[a_rule-1], (1-pos_support)))

    # === Card > 1 rules ===
    for a_card in range(2, max_card+1):
        all_rules_a_card = combinations(one_rules, a_card)
        for a_rule in all_rules_a_card:
            # Check (initial) minimum support
            rule_capts = np.ones(X.shape[0])
            for a_one_rule in a_rule:
                if a_one_rule > 0:
                    rule_capts = np.logical_and(rule_capts, X[:,a_one_rule-1])
                else:
                    rule_capts = np.logical_and(rule_capts, np.logical_not(X[:,(-a_one_rule)-1]))

            rule_support = np.mean(rule_capts)

            if rule_support >= min_support and rule_support <= 1 - min_support:
                list_of_rules.append(list(a_rule))
                tot_rules += 1
                if "mine" in verbosity:
                    print("(%d) %s with support %.6f" %(tot_rules, get_rule_str(a_rule, features), rule_support))

    return list_of_rules, tot_rules

def rule_indices(a_rule, X):
    """
    Returns an array of indexes that represent the samples captured by a_rule.
    """
    rule_capts = np.ones(X.shape[0])
    
    for a_one_rule in a_rule:
        if a_one_rule > 0:
            rule_capts = np.logical_and(rule_capts, X[:,a_one_rule-1])
        else:
            rule_capts = np.logical_and(rule_capts, np.logical_not(X[:,(-a_one_rule)-1]))

    return np.where(rule_capts == 1)

 
 
