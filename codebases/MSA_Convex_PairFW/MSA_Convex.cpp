/*################################################################ 
## MODULE: MSA_Convex.cpp
## VERSION: 1.0 
## SINCE 2015-09-01
##      
#################################################################
## Edited by MacVim
## Class Info auto-generated by Snippet 
################################################################*/

#include "MSA_Convex.h"

/* Debugging option */
// #define RECURSION_TRACE
// #define FIRST_SUBPROBLEM_DEBUG
// #define SECOND_SUBPROBLEM_DEBUG

void usage () { cout << "./MSA_Convex (options) [seq_file]" << endl;
    cout << "seq_file should contain one or more DNA sequence. " << endl;
    cout << "Options: " << endl;
    cout << "\t-l Set the maximum length of model sequence. (default 0)" << endl;
    cout << "\t-m Set step size (\\mu) for updating the ADMM coordinate variables. (default 0.1)"<< endl;
    cout << "\t-p Set maximum pertubation of penalty to break ties. (default 0)"<< endl;
    cout << "\t-s Set ADMM early stop toggle: early stop (on) if > 0. (default on)"<< endl;
    cout << "\t-r Set whether reinitialize W_1 and W_2 at each ADMM iteration. (default off)"<< endl;
}

class AtomHasher
{
    public:
        size_t operator() (vector<int> const& key) const {
            int result = 0;
            int shift = 0;
            for (int i = 0; i < key.size(); i++) {
                shift = (shift + 11) % 21;
                result ^= (key[i]+1024) << shift;
            }
            return result;
        }
};
class AtomEqualFn
{
    public:
        bool operator() (vector<int> const& t1, vector<int> const& t2) const {
            int t1_size = t1.size(), t2_size = t2.size();
            if (t1_size != t2_size) return false;
            for (int i = 0; i < t1_size; i ++) 
                if (t1[i] != t2[i]) return false;
            return true;
        }
};

void parse_cmd_line (int argn, char** argv) {
    if (argn < 2) { 
        usage();
        exit(0);
    }
    int i;
    for(i = 1; i < argn; i++){
        if ( argv[i][0] != '-' ) break;
        if ( ++i >= argn ) usage();
        switch(argv[i-1][1]){
            case 'e': ADMM_EARLY_STOP_TOGGLE = (atoi(argv[i])>0); break;
            case 'r': REINIT_W_ZERO_TOGGLE = (atoi(argv[i])>0); break;
            case 'l': LENGTH_OFFSET = atoi(argv[i]); break;
            case 'm': MU = atof(argv[i]); break;
            case 'p': PERB_EPS = atof(argv[i]); break;
            default:
                      cerr << "unknown option: -" << argv[i-1][1] << endl;
                      usage();
                      exit(0);
        }
    }
    if (i >= argn) usage();
    trainFname = argv[i];
}

void parse_seqs_file (SequenceSet& allSeqs, int& numSeq, char* fname) {
    ifstream seq_file(fname);
    string tmp_str;
    while (getline(seq_file, tmp_str)) {
        int seq_len = tmp_str.size();
        Sequence ht_tmp_seq (seq_len+1+1, 0);
        ht_tmp_seq[0] = '*';
        for(int i = 0; i < seq_len; i ++) 
            ht_tmp_seq[i+1] = tmp_str.at(i);
        ht_tmp_seq[seq_len+1] = '#';
        allSeqs.push_back(ht_tmp_seq);
        ++ numSeq;
    }
    seq_file.close();
}

int get_init_model_length (vector<int>& lenSeqs) {
    int max_seq_length = -1;
    int numSeq = lenSeqs.size(); 
    for (int i = 0; i < numSeq; i ++)
        if (lenSeqs[i] > max_seq_length) 
            max_seq_length = lenSeqs[i];
    return max_seq_length;
}

double get_sub1_cost (Tensor5D& W, Tensor5D& Z, Tensor5D& Y, Tensor5D& C, double& mu, SequenceSet& allSeqs) {
    int numSeq = W.size();
    int T2 = W[0].size();
    double lin_term = 0.0, qua_term = 0.0;
    for (int n = 0; n < numSeq; n ++) {
        int T1 = W[n].size();
        for (int i = 0; i < T1; i ++) 
            for (int j = 0; j < T2; j ++) 
                for (int d = 0; d < NUM_DNA_TYPE; d ++) 
                    for (int m = 0; m < NUM_MOVEMENT; m ++) {
                        double sterm = W[n][i][j][d][m] - Z[n][i][j][d][m] + 1.0/mu*Y[n][i][j][d][m];
                        lin_term += C[n][i][j][d][m] * W[n][i][j][d][m];
                        qua_term += 0.5 * mu * sterm * sterm;
                    }
    }
    return lin_term + qua_term;
}

double get_sub2_cost (Tensor5D& W, Tensor5D& Z, Tensor5D& Y, double& mu, SequenceSet& allSeqs) {
    int numSeq = W.size();
    int T2 = W[0].size();
    double qua_term = 0.0;
    for (int n = 0; n < numSeq; n ++) {
        int T1 = W[n].size();
        for (int i = 0; i < T1; i ++) 
            for (int j = 0; j < T2; j ++) 
                for (int d = 0; d < NUM_DNA_TYPE; d ++) 
                    for (int m = 0; m < NUM_MOVEMENT; m ++) {
                        double sterm = W[n][i][j][d][m] - Z[n][i][j][d][m] + 1.0/mu*Y[n][i][j][d][m];
                        qua_term += sterm * sterm;
                    }
    }
    return qua_term;
}

double first_subproblem_log (int fw_iter, Tensor4D& W_1, Tensor4D& W_2, Tensor4D& Y, Tensor4D& C, double mu) {
    /*{{{*/
    double cost = 0.0, lin_term = 0.0, qua_term = 0.0;
    int T1 = W_1.size();
    int T2 = W_1[0].size();
    for (int i = 0; i < T1; i ++) 
        for (int j = 0; j < T2; j ++) 
            for (int d = 0; d < NUM_DNA_TYPE; d ++) 
                for (int m = 0; m < NUM_MOVEMENT; m ++) {
                    double sterm =  (W_1[i][j][d][m] - W_2[i][j][d][m] + 1.0/mu*Y[i][j][d][m]);
                    lin_term += C[i][j][d][m] * W_1[i][j][d][m];
                    qua_term += 0.5*mu *sterm * sterm;
                }
    cost = lin_term + qua_term;
    cout << "[FW1] iter=" << fw_iter
        << ", lin_term: " << lin_term 
        << ", qua_sterm: " << qua_term
        << ", cost=" << cost 
        << endl;
    /*}}}*/
}

double second_subproblem_log (int fw_iter, Tensor5D& W, Tensor5D& Z, Tensor5D& Y, double mu) {
    /*{{{*/
    double cost = 0.0,  qua_term = 0.0;
    int numSeq = W.size();
    double Ws = 0.0;
    for (int n = 0; n < numSeq; n ++) 
        Ws += tensor4D_frob_prod (W[n], W[n]); 
    for (int n = 0; n < numSeq; n ++) {
        int T1 = W[n].size();
        int T2 = W[n][0].size();
        for (int i = 0; i < T1; i ++) 
            for (int j = 0; j < T2; j ++) 
                for (int d = 0; d < NUM_DNA_TYPE; d ++) 
                    for (int m = 0; m < NUM_MOVEMENT; m ++) {
                        double sterm =  (W[n][i][j][d][m] - Z[n][i][j][d][m] - 1.0/mu*Y[n][i][j][d][m]);
                        qua_term += 0.5*mu *sterm * sterm;
                    }
    }
    cost = qua_term;
    cout << "[FW2] iter=" << fw_iter 
        << ", ||W||^2: " << Ws  
        << ", qua_sterm: " << qua_term
        << ", cost=" << cost  
        << endl;
    /*}}}*/
}

void dump_4Datom (vector<int> atom) {
    for (int p = 0; p < atom.size(); p+=4 ) {
        int i = atom[p], j = atom[p+1], d = atom[p+2], m = atom[p+3];
        cout << "(" << i << "," << j << "," << d << "," << m << ")->";
    }
    cout << endl;
    return;
}

/* We resolve the first subproblem through the frank-wolfe algorithm */
void first_subproblem (Tensor4D& W_1, Tensor4D& W_2, Tensor4D& Y, Tensor4D& C, double& mu, Sequence data_seq) {
    /*{{{*/
    // 1. Find the update direction
    int T1 = W_1.size();
    int T2 = W_1[0].size();
    Tensor4D M (T1, Tensor(T2, Matrix(NUM_DNA_TYPE, vector<double>(NUM_MOVEMENT, 0.0)))); 
    // reinitialize to all-zero matrix
    for (int i = 0; i < T1; i ++) 
        for (int j = 0; j < T2; j ++) 
            for (int d = 0; d < NUM_DNA_TYPE; d ++) 
                for (int m = 0; m < NUM_MOVEMENT; m ++) {
                    W_1[i][j][d][m] = 0.0;
                    M[i][j][d][m] = Y[i][j][d][m] - mu* W_2[i][j][d][m]; 
                }

    int fw_iter = -1;
    unordered_map < vector<int> , double, AtomHasher, AtomEqualFn > alpha_lookup;
     // first_subproblem_log(fw_iter, W_1, W_2, Y, C, mu);
     bool toexit = false;
    while (fw_iter < MAX_1st_FW_ITER) {
        fw_iter ++;
        // find atom S for FW direction 
        vector<int> S_atom;
        Trace trace (0, Cell(3));
        cube_smith_waterman (S_atom, trace, M, C, data_seq);
        // for (int i = 0; i < trace.size(); i ++)
         //   cout << trace[i].toString() << endl;
        double gfw_S = 0.0, gfw_W = 0.0;
        for ( auto& x: alpha_lookup) {
            for (int p = 0; p < x.first.size(); p+=4 ) {
                int i = x.first[p], j = x.first[p+1], d = x.first[p+2], m = x.first[p+3];
                gfw_W += (C[i][j][d][m]+M[i][j][d][m]) * x.second;
            }
        }
        for (int p = 0; p < S_atom.size(); p+=4 ) {
            int i = S_atom[p], j = S_atom[p+1], d = S_atom[p+2], m = S_atom[p+3];
            gfw_S -= C[i][j][d][m]+M[i][j][d][m];
        }
        double gfw = gfw_S + gfw_W;
        // dump_4Datom (S_atom);
        if (fw_iter > 0 && gfw < 0) {
            cout << "GFW_1_W: " << gfw_W << ", GFW_1_S: " << gfw_S << ", GFW: " << gfw << endl;
            toexit = true;
            // exit(-1);
        }
        if (fw_iter > 0 && (gfw < GFW_EPS)) break;

        // find atom V for away direction 
        vector<int> V_atom;
        double gamma_max = 1.0;
        if (alpha_lookup.size() > 0) {
            double max_val = -1e99;
            for ( auto& x: alpha_lookup) {
                double val = 0.0;
                for (int p = 0; p < x.first.size(); p+=4 ) {
                    int i = x.first[p], j = x.first[p+1], d = x.first[p+2], m = x.first[p+3];
                    val += C[i][j][d][m] + M[i][j][d][m];
                }
                if (val > max_val) {
                    max_val = val; 
                    V_atom = x.first; 
                    gamma_max = x.second;
                }
            }
        }
        // dump_4Datom (V_atom);
       //  cout << "alpha_v_atom: " << alpha_lookup[V_atom] << endl;

        // 2. Exact Line search: determine the optimal step size \gamma
        // gamma = [ ( mu*W_2 - mu*W_1 - Y - C ) dot (S-V) ] / (mu* || S-V ||^2)
        double numerator = 0.0, denominator = 0.0;
        unordered_map < vector<int> , double, AtomHasher, AtomEqualFn > smv_lookup;
        for (int p = 0; p < S_atom.size(); p+=4 ) {
            int i = S_atom[p], j = S_atom[p+1], d = S_atom[p+2], m = S_atom[p+3];
            numerator += (mu*(W_2[i][j][d][m] - W_1[i][j][d][m])- C[i][j][d][m] - Y[i][j][d][m]);
            vector<int> state (S_atom.begin()+p, S_atom.begin()+p+4);
            pair<vector<int>, double> state_pair (state, 1.0);
            smv_lookup.insert(state_pair); 
            denominator += mu;
        }
        for (int p = 0; p < V_atom.size(); p+=4 ) {
            int i = V_atom[p], j = V_atom[p+1], d = V_atom[p+2], m = V_atom[p+3];
            numerator -= (mu*(W_2[i][j][d][m] - W_1[i][j][d][m])- C[i][j][d][m] - Y[i][j][d][m]);
            vector<int> state (V_atom.begin()+p, V_atom.begin()+p+4);
            if (smv_lookup.find(state) == smv_lookup.end()) denominator += mu;
            else denominator -= mu;
        }
        // 3a. early stop condition: neglible denominator
        double gamma = numerator/denominator;
        gamma = (REINIT_W_ZERO_TOGGLE && fw_iter == 0)?1.0:gamma;
        gamma = min(max(gamma, 0.0), gamma_max);

        // cout << "gamma:" << gamma << endl;
        // 4. update W_1
        for (int p = 0; p < S_atom.size(); p+=4 ) {
            int i = S_atom[p], j = S_atom[p+1], d = S_atom[p+2], m = S_atom[p+3];
            W_1[i][j][d][m] += gamma;
            M[i][j][d][m] += mu * gamma;
        }
        for (int p = 0; p < V_atom.size(); p+=4 ) {
            int i = V_atom[p], j = V_atom[p+1], d = V_atom[p+2], m = V_atom[p+3];
            W_1[i][j][d][m] -= gamma;
            M[i][j][d][m] -= mu * gamma;
        }

        if (alpha_lookup.size() == 0) {
            pair<vector<int>,double> new_item(S_atom, 1.0);
            alpha_lookup.insert(new_item);
           // cout << "gamma = " << gamma << ", init_insert. " << endl;
        } else {
            alpha_lookup[S_atom] += gamma;
            if (alpha_lookup[V_atom] - gamma < 1e-10) alpha_lookup.erase(V_atom);
            else alpha_lookup[V_atom] -= gamma;
            // cout << "gamma = " << gamma << ", update " << endl;
        }
/*
         cout << "alpha_look: ";
         for ( auto& x: alpha_lookup) cout << x.second << ",";
        cout << endl;
        */
        // 5. output iteration tracking info
         // first_subproblem_log(fw_iter, W_1, W_2, Y, C, mu);
         // cout << endl;
         // if (toexit) exit(-1);
    }
    return; 
    /*}}}*/
}

/* We resolve the second subproblem through sky-plane projection */
void second_subproblem (Tensor5D& W_1, Tensor5D& W_2, Tensor5D& Y, double& mu, SequenceSet& allSeqs, vector<int> lenSeqs) {
    /*{{{*/
    int numSeq = allSeqs.size();
    int T2 = W_2[0][0].size();
    // initialization
    vector<Tensor4D> delta (numSeq, Tensor4D(0, Tensor(T2, Matrix(NUM_DNA_TYPE,
                        vector<double>(NUM_MOVEMENT, 0.0)))));  
    tensor5D_init (delta, allSeqs, lenSeqs, T2);
    Tensor tensor (T2, Matrix (NUM_DNA_TYPE, vector<double>(NUM_DNA_TYPE, 0.0)));
    Matrix mat_insertion (T2, vector<double>(NUM_DNA_TYPE, 0.0));
    for (int n = 0;  n < numSeq; n ++) 
        for (int i = 0; i < W_2[n].size(); i ++)  
            for (int j = 0; j < T2; j ++) 
                for (int d = 0; d < NUM_DNA_TYPE; d ++) 
                    for (int m = 0; m < NUM_MOVEMENT; m ++) {
                        W_2[n][i][j][d][m] = 0.0;
                        delta[n][i][j][d][m] = mu * W_1[n][i][j][d][m] + Y[n][i][j][d][m];
                        if (m == INSERTION) mat_insertion[j][d] += max(0.0, delta[n][i][j][d][m]);
                        else tensor[j][d][move2T3idx(m)] += max(0.0, delta[n][i][j][d][m]);
                    }
    int fw_iter = -1;
    unordered_map < vector<int> , double, AtomHasher, AtomEqualFn > alpha_lookup;
    while ( fw_iter < MAX_2nd_FW_ITER) {
        fw_iter ++;
        // 2. determine the trace: run viterbi algorithm
        Trace trace (0, Cell(2)); // 1d: j, 2d: ATCG
        refined_viterbi_algo (trace, tensor, mat_insertion);
        vector<int> S_atom;
        // 3. recover values for S 
        // 3b. set a number of selected elements to 1
        cout << "Rev: ";
        bool see_end = false;
        for (int t = 0; t < trace.size(); t++) {
            int sj = trace[t].location[0];
            int sd = trace[t].location[1];
            int sm = dna2T3idx(trace[t].acidB);
            if(trace[t].acidA == '#') break;
            cout << trace[t].acidB;
            for (int n = 0; n < numSeq; n ++) 
                for (int i = 0; i < delta[n].size(); i ++) 
                    for (int m = 0; m < NUM_MOVEMENT; m ++)
                        if (delta[n][i][sj][sd][m] > 0.0) { 
                            bool added = false;
                            if (m == DEL_BASE_IDX + sm || m == MTH_BASE_IDX + sm) added = true;
                            else if (m == INSERTION && trace[t].action == INSERTION) added = true;
                            if (added) {
                                S_atom.push_back(n);
                                S_atom.push_back(i);
                                S_atom.push_back(sj);
                                S_atom.push_back(sd);
                                S_atom.push_back(m);
                            }
                        }
        }
        cout <<  endl;
        // early stopping
        double gfw = 0.0;
        for (int p = 0; p < S_atom.size(); p+=5 ) {
            int n = S_atom[p], i = S_atom[p+1], j = S_atom[p+2], d = S_atom[p+3], m = S_atom[p+4];
            gfw += delta[n][i][j][d][m];
        }
        for ( auto& x: alpha_lookup) {
            for (int p = 0; p < x.first.size(); p+=5 ) {
                int n = x.first[p], i = x.first[p+1], j = x.first[p+2], d = x.first[p+3], m = x.first[p+4];
                gfw -= delta[n][i][j][d][m] * x.second;
            }
        }
        /*
        if (fw_iter > 0 && gfw < 0) {
            cout << "ISSUE: GFW_2: " << gfw << endl;
            // exit(-1);
        }
        */
        if (fw_iter > 0 && (gfw < GFW_EPS)) {
           // cout << "break; " << endl;
            break;
        }
        // away step
        vector<int> V_atom;
        double gamma_max = 1.0;
        if (alpha_lookup.size() > 0) {
            double min_val = 1e99;
            for ( auto& x: alpha_lookup) {
                double val = 0.0;
                for (int p = 0; p < x.first.size(); p+=5 )
                    val += delta[x.first[p]][x.first[p+1]][x.first[p+2]][x.first[p+3]][x.first[p+4]];
                if (val < min_val) {
                    min_val = val; 
                    V_atom = x.first; 
                    gamma_max = x.second;
                }
            }
        }
        // 4. Exact Line search: determine the optimal step size \gamma
        // gamma = [ ( W_1 - W_2 + 1/mu*Y ) dot (S - V) ] / || S-V ||^2
        //           ---------------combo------------------
        double numerator = 0.0, denominator = 0.0;
        unordered_map < vector<int> , double, AtomHasher, AtomEqualFn > smv_lookup;
        for (int p = 0; p < S_atom.size(); p+=5 ) {
            int n = S_atom[p], i = S_atom[p+1], j = S_atom[p+2], d = S_atom[p+3], m = S_atom[p+4];
            numerator += (1.0/mu)*Y[n][i][j][d][m] + W_1[n][i][j][d][m] - W_2[n][i][j][d][m];
            vector<int> state (S_atom.begin()+p, S_atom.begin()+p+5);
            pair<vector<int>, double> state_pair (state, 1.0);
            smv_lookup.insert(state_pair); 
            denominator += mu;
        }
        for (int p = 0; p < V_atom.size(); p+=5 ) {
            int n = V_atom[p], i = V_atom[p+1], j = V_atom[p+2], d = V_atom[p+3], m = V_atom[p+4];
            numerator -= (1.0/mu)*Y[n][i][j][d][m] + W_1[n][i][j][d][m] - W_2[n][i][j][d][m];
            vector<int> state (V_atom.begin()+p, V_atom.begin()+p+5);
            if (smv_lookup.find(state) == smv_lookup.end()) denominator += mu;
            else denominator -= mu;
        }
        double gamma;
        gamma = (denominator < 10e-6)?gamma_max:(numerator/denominator);
        gamma = (REINIT_W_ZERO_TOGGLE && fw_iter == 0)?1.0:gamma;
        gamma = min(max(gamma, 0.0), gamma_max);
        // 3. update W_2
        // delta[n][i][j][d][m] = mu * (W_1[n][i][j][d][m]-W_2[n][i][j][d][m]) + Y[n][i][j][d][m];
        for (int p = 0; p < S_atom.size(); p+=5 ) {
            int n = S_atom[p], i = S_atom[p+1], j = S_atom[p+2], d = S_atom[p+3], m = S_atom[p+4];
            W_2[n][i][j][d][m] += gamma;
            if (m == INSERTION) mat_insertion[j][d] -= max(0.0, delta[n][i][j][d][m]);
            else tensor[j][d][move2T3idx(m)] -= max(0.0, delta[n][i][j][d][m]);
            delta[n][i][j][d][m] -= mu * gamma;
            if (m == INSERTION) mat_insertion[j][d] += max(0.0, delta[n][i][j][d][m]);
            else tensor[j][d][move2T3idx(m)] += max(0.0, delta[n][i][j][d][m]);
        }
        for (int p = 0; p < V_atom.size(); p+=5 ) {
            int n = V_atom[p], i = V_atom[p+1], j = V_atom[p+2], d = V_atom[p+3], m = V_atom[p+4];
            W_2[n][i][j][d][m] -= gamma; 
            if (m != INSERTION) tensor[j][d][move2T3idx(m)] -= max(0.0, delta[n][i][j][d][m]);
            else mat_insertion[j][d] -= max(0.0, delta[n][i][j][d][m]);
            double val = mu *gamma; delta[n][i][j][d][m] += val;
            if (m != INSERTION) tensor[j][d][move2T3idx(m)] += max(0.0, delta[n][i][j][d][m]);
            else mat_insertion[j][d] += max(0.0, delta[n][i][j][d][m]);
        }
        if (alpha_lookup.size() == 0) {
            pair<vector<int>,double> new_item(S_atom, 1.0);
            alpha_lookup.insert(new_item);
            // cout << ", gamma = " << gamma << ", init_insert. " << endl;
        } else {
            alpha_lookup[S_atom] += gamma;
            if (alpha_lookup[V_atom] - gamma < 1e-6) alpha_lookup.erase(V_atom);
            else alpha_lookup[V_atom] -= gamma;
            //  cout << ", gamma = " << gamma << ", update " << endl;
        }
        // 4. output iteration tracking info
         // second_subproblem_log(fw_iter, W_2, W_1, Y, mu);
    }
    return;
    /*}}}*/
}

Tensor5D CVX_ADMM_MSA (SequenceSet& allSeqs, vector<int>& lenSeqs, int T2) {
    /*{{{*/
    // 1. initialization
    int numSeq = allSeqs.size();
    vector<Tensor4D> C (numSeq, Tensor4D(0, Tensor(T2, Matrix(NUM_DNA_TYPE,
                        vector<double>(NUM_MOVEMENT, 0.0)))));  
    vector<Tensor4D> W_1 (numSeq, Tensor4D(0, Tensor(T2, Matrix(NUM_DNA_TYPE,
                        vector<double>(NUM_MOVEMENT, 0.0)))));  
    vector<Tensor4D> W_2 (numSeq, Tensor4D(0, Tensor(T2, Matrix(NUM_DNA_TYPE,
                        vector<double>(NUM_MOVEMENT, 0.0)))));  
    vector<Tensor4D> Y (numSeq, Tensor4D(0, Tensor(T2, Matrix(NUM_DNA_TYPE,
                        vector<double>(NUM_MOVEMENT, 0.0)))));  
    tensor5D_init (C, allSeqs, lenSeqs, T2);
    tensor5D_init (W_1, allSeqs, lenSeqs, T2);
    tensor5D_init (W_2, allSeqs, lenSeqs, T2);
    tensor5D_init (Y, allSeqs, lenSeqs, T2);
    set_C (C, allSeqs);

    // 2. ADMM iteration
    int iter = 0;
    double mu = MU;
    double prev_CoZ = MAX_DOUBLE;
    while (iter < MAX_ADMM_ITER) {
        // 2a. Subprogram: FrankWolf Algorithm
        // NOTE: parallelize this for to enable parallelism
#ifdef PARRALLEL_COMPUTING
#pragma omp parallel for
#endif
        for (int n = 0; n < numSeq; n++) 
            first_subproblem (W_1[n], W_2[n], Y[n], C[n], mu, allSeqs[n]);

        // 2b. Subprogram: 
        second_subproblem (W_1, W_2, Y, mu, allSeqs, lenSeqs);

        // 2d. update Y: Y += mu * (W_1 - W_2)
        for (int n = 0; n < numSeq; n ++)
            tensor4D_lin_update (Y[n], W_1[n], W_2[n], mu);

        // 2e. print out tracking info
        double CoZ = 0.0;
        for (int n = 0; n < numSeq; n++) 
            CoZ += tensor4D_frob_prod(C[n], W_2[n]);
        double W1mW2 = 0.0;
        for (int n = 0; n < numSeq; n ++) {
            int T1 = W_1[n].size();
            for (int i = 0; i < T1; i ++) 
                for (int j = 0; j < T2; j ++) 
                    for (int d = 0; d < NUM_DNA_TYPE; d ++) 
                        for (int m = 0; m < NUM_MOVEMENT; m ++) {
                            double value = (W_1[n][i][j][d][m] - W_2[n][i][j][d][m]);
                            W1mW2 = max( fabs(value), W1mW2 ) ;
                        }
        }
        // cerr << "=============================================================================" << endl;
        char COZ_val [50], w1mw2_val [50]; 
        sprintf(COZ_val, "%6f", CoZ);
        sprintf(w1mw2_val, "%6f", W1mW2);
        cerr << "ADMM_iter = " << iter 
            << ", C o Z = " << COZ_val
            << ", || W_1 - W_2 ||_{max} = " << w1mw2_val
            << endl;
        // cerr << "sub1_Obj = CoW_1+0.5*mu*||W_1-Z+1/mu*Y_1||^2 = " << sub1_cost << endl;
        // cerr << "sub2_Obj = ||W_2-Z+1/mu*Y_2||^2 = " << sub2_cost << endl;

        // 2f. stopping conditions
        if (ADMM_EARLY_STOP_TOGGLE and iter > MIN_ADMM_ITER)
            if ( W1mW2 < EPS_Wdiff ) {
                cerr << "CoZ Converges. ADMM early stop!" << endl;
                break;
            }
        prev_CoZ = CoZ;
        iter ++;
    }
    cout << "W_1: " << endl;
    for (int i = 0; i < numSeq; i ++) tensor4D_dump(W_1[i]);
    cout << "W_2: " << endl;
    for (int i = 0; i < numSeq; i ++) tensor4D_dump(W_2[i]);
    return W_2;
    /*}}}*/
}

void sequence_dump (SequenceSet& allSeqs, int n) {
    char buffer [50];
    sprintf (buffer, "Seq%5d", n);
    cout << buffer << ": ";
    for (int j = 0; j < allSeqs[n].size(); j ++) 
        cout << allSeqs[n][j];
    cout << endl;
}

int main (int argn, char** argv) {
    // 1. parse cmd 
    parse_cmd_line(argn, argv);
    // 2. input DNA sequence file
    int numSeq = 0;
    SequenceSet allSeqs (0, Sequence());
    parse_seqs_file(allSeqs, numSeq, trainFname);
    vector<int> lenSeqs (numSeq, 0);
    for (int n = 0; n < numSeq; n ++) 
        lenSeqs[n] = allSeqs[n].size();
    int T2 = get_init_model_length (lenSeqs) + LENGTH_OFFSET; // model_seq_length
    // pre-info
    cout << "#########################################################" << endl;
    cout << "ScoreMatch: " << C_M;
    cout << ", ScoreInsertion: " << C_I;
    cout << ", ScoreDeletion: " << C_D;
    cout << ", ScoreMismatch: " << C_MM << endl;
    cout << "PERB_EPS: " << PERB_EPS;
    cout << ", GFW_EPS: " << GFW_EPS;
    cout << ", LENGTH_OFFSET: " << LENGTH_OFFSET;
    cout << ", EPS_Wdiff: " << EPS_Wdiff << endl;
    for (int n = 0; n < numSeq; n ++) 
        sequence_dump(allSeqs, n);

    // 3. relaxed convex program: ADMM-based algorithm
    // omp_set_num_threads(NUM_THREADS);
    time_t begin = time(NULL);
    vector<Tensor4D> W = CVX_ADMM_MSA (allSeqs, lenSeqs, T2);
    time_t end = time(NULL);

    // 4. output the result
    // a. tuple view
    cout << ">>>>>>>>>>>>>>>>>>>>>>>TupleView<<<<<<<<<<<<<<<<<<<<<<<<" << endl;
    for (int n = 0; n < numSeq; n ++) {
        cout << "n = " << n << endl;
        tensor4D_dump(W[n]);
    }
    // b. sequence view
    cout << ">>>>>>>>>>>>>>>>>>>>>>>SequenceView<<<<<<<<<<<<<<<<<<<<<<<<" << endl;
    int T2m = T2;
    Tensor tensor (T2m, Matrix (NUM_DNA_TYPE, vector<double>(NUM_DNA_TYPE, 0.0)));
    Matrix mat_insertion (T2m, vector<double> (NUM_DNA_TYPE, 0.0));
    for (int n = 0; n < numSeq; n ++) {
        int T1 = W[n].size();
        for (int i = 0; i < T1; i ++) { 
            for (int j = 0; j < T2m; j ++) {
                for (int d = 0; d < NUM_DNA_TYPE; d ++) {
                    for (int m = 0; m < NUM_MOVEMENT; m ++) {

                        if (m == INSERTION) mat_insertion[j][d] += max(0.0, W[n][i][j][d][m]);
                        else tensor[j][d][move2T3idx(m)] += max(0.0, W[n][i][j][d][m]);
                    }
                }
            }
        }
    }
    Trace trace (0, Cell(2)); // 1d: j, 2d: ATCG
    refined_viterbi_algo (trace, tensor, mat_insertion);
    // for (int i = 0; i < trace.size(); i ++) 
    //    cout << trace[i].toString() << endl;
    for (int n = 0; n < numSeq; n ++) {
        char buffer [50];
        sprintf (buffer, "Seq%5d", n);
        cout << buffer << ": ";
        for (int j = 0; j < allSeqs[n].size(); j ++) 
            cout << allSeqs[n][j];
        cout << endl;
    }
    Sequence recSeq;
    cout << "SeqRecov: ";
    for (int i = 0; i < trace.size(); i ++) 
        if (trace[i].action != INSERTION) {
            cout << trace[i].acidB;
            recSeq.push_back(trace[i].acidB);
            if (trace[i].acidB == '#') break;
        }
    cout << endl;
    cout << ">>>>>>>>>>>>>>>>>>>>>>>MatchingView<<<<<<<<<<<<<<<<<<<<<<<<" << endl;
    // NOTE: rounding scheme
    SequenceSet allModelSeqs, allDataSeqs;
    for (int n = 0; n < numSeq; n ++) {
        Sequence model_seq = recSeq, data_seq = allSeqs[n];
       data_seq.erase(data_seq.begin());
       model_seq.erase(model_seq.begin());
        data_seq.erase(data_seq.end()-1);
        model_seq.erase(model_seq.end()-1);

        // align sequences locally
        Plane plane (data_seq.size()+1, Trace(model_seq.size()+1, Cell(2)));
        Trace trace (0, Cell(2));
        smith_waterman (model_seq, data_seq, plane, trace);

        // 4. output the result
        model_seq.clear(); data_seq.clear();
        for (int i = 0; i < trace.size(); i ++) 
            model_seq.push_back(trace[i].acidA);
        for (int i = 0; i < trace.size(); i ++) 
            data_seq.push_back(trace[i].acidB);
        allModelSeqs.push_back(model_seq);
        allDataSeqs.push_back(data_seq);
        for (int i = 0; i < model_seq.size(); i ++) cout << model_seq[i];
        cout << endl;
        for (int i = 0; i < data_seq.size(); i ++) cout << data_seq[i];
        cout << endl;
    }
    cout << ">>>>>>>>>>>>>>>>>>>>>ClustalOmegaView<<<<<<<<<<<<<<<<<<<<<<" << endl;
    SequenceSet allCOSeqs (numSeq, Sequence(0));
    vector<int> pos(numSeq, 0);
    while (true) {
        set<int> insertion_ids;
        for (int i = 0; i < numSeq; i ++) {
            if (pos[i] >= allModelSeqs[i].size()) continue;
            if (allModelSeqs[i][pos[i]] == '-') 
                insertion_ids.insert(i);
        }
        if (insertion_ids.size() != 0) {
            // insertion exists
            for (int i = 0; i < numSeq; i ++) {
                if (insertion_ids.find(i)==insertion_ids.end()) // not in set
                    allCOSeqs[i].push_back('-');
                else { // in set
                    allCOSeqs[i].push_back(allDataSeqs[i][pos[i]++]);
                }
            }
        } else { // no insertion
            for (int i = 0; i < numSeq; i ++) 
                allCOSeqs[i].push_back(allDataSeqs[i][pos[i]++]);
        }
        // terminating
        bool terminated = true;
        for (int i = 0; i < numSeq; i ++) 
            if (pos[i] != allModelSeqs[i].size()) {
                terminated = false; 
                break;
            }
        if (terminated) break;
    }
    string fname (trainFname);
    fname = fname + ".co";
    ofstream co_out (fname.c_str());
    for (int i = 0; i < numSeq; i ++) {
        for (int j = 0; j < allCOSeqs[i].size(); j++)  {
            co_out << allCOSeqs[i][j];
            cout << allCOSeqs[i][j];
        }
        co_out << endl;
        cout << endl;
    }
    co_out.close();
    cout << "#########################################################" << endl;
    cout << "Time Spent: " << end - begin << " seconds" << endl;
    return 0;
}
