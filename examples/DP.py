import numpy as np
from scipy import integrate, optimize
import time
np.seterr(all="ignore")

rng = np.random.default_rng(42)
#print(rng.bit_generator.state)

def set_seed(seed):
    global rng
    rng = np.random.default_rng(seed)

def clean_dataset(X,features, biases):
    """Returns a dataset deprived from the columns we do not want the classifier rules to be based upon"""
    
    if biases is None : return X, features
    rmv = np.zeros(len(features), dtype=int)
    for bias in biases :
        rmv += np.fromiter(map(lambda x: 1 if x.startswith(bias) else 0, features), dtype=int)
    
    rmv_idx = np.where(rmv == 1)[0]
    return np.delete(X, rmv_idx, axis = 1), [feature for (idx,feature) in enumerate(features) if idx not in rmv_idx]
    

def get_biases(dataset):
    if dataset=="compas" : return unbias_compas
    elif dataset == "adult": return unbias_adult
    #elif dataset == "folktable" : return unbias_folktable
    
    else : return None
    
unbias_compas=["Race", "Age", "Gender"]
unbias_adult = ["gender", "age"]
unbias_folktable = ["RAC1P", "SEX", "POBP", "OCCP", "AGEP"] #pobp = place of birth | occp : occupation, seems more specific than cow = class of worker


def split_dataset(X, y, split, seed):
    start= time.time()
    set_seed(seed)
    N = len(X)
    split = int(N*split)
    perm = rng.permutation(N)
    X,y = X[perm], y[perm]    
    return X[:split], y[:split], X[split:], y[split:]       
        
        
        
