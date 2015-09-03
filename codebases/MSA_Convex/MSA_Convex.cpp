/*###############################################################
## MODULE: MSA_Convex.cpp
## VERSION: 1.0 
## SINCE 2015-09-01
## DESCRIPTION: 
##      
#################################################################
## Edited by MacVim
## Class Info auto-generated by Snippet 
################################################################*/


#include "MSA_Convex.h"


/* Debugging option */
//#define RECURSION_TRACE

/* Define Scores of all Actions */
const double MATCH_SCORE = +2;
const double MISMATCH_SCORE = -1;
const double INSERTION_SCORE = -1;
const double DELETION_SCORE = -1;
const char GAP_NOTATION = '-';

/* 
   The first sequence is observed. 
   The second sequence is the one to be aligned with the observed one.
   */
void usage () { cout << "./PSA_CUBE [seq_file]" << endl;
    cout << "seq_file should contain two DNA sequence in its first line and second line. " << endl;
    cout << "The first sequence is observed. " << endl;
    cout << "The second sequence is the one to be aligned with the observed one." << endl;
}

/* FrankWolf Algorithm */
/*{{{*/
//void FrankWolf () {
//    int nRow = tensor.size();
//    int nCol = tensor[0].size();
//    int nDep = tensor[0][0].size();
//    // 1. fill in the tensor
//    double max_score = MIN_DOUBLE;
//    int max_i = -1, max_j = -1;
//    for (int i = 0; i < nRow; i ++) {
//        for (int j = 0; j < nCol; j ++) {
//            for (int k = 0; k < nDep; k ++) {
//                tensor[i][j][k].row_index = i;
//                tensor[i][j][k].col_index = j;
//                tensor[i][j][k].dep_index = k;
//                if (i == 0 or j == 0 or k == 0) continue;
//                char acidA = seqA[j-1];
//                char acidB = seqB[i-1];
//                // 1a. get max matach/mismatch score
//                double mscore = isMatch2(acidA,acidB)?MATCH_SCORE:MISMATCH_SCORE;
//                double mm_score = tensor[i-1][j-1][].score + mscore;
//                // 1b. get max insertion score
//                double ins_score = MIN_DOUBLE;
//                for (int l = 1; j - l > 0 ; l ++) 
//                    ins_score = max(ins_score, tensor[i][j-l].score + l * INSERTION_SCORE);
//                double del_score = MIN_DOUBLE;
//                // 1c. get max deletion score
//                for (int l = 1; i - l > 0 ; l ++) 
//                    del_score = max(del_score, tensor[i-l][j].score + l * DELETION_SCORE);
//                double opt_score = MIN_DOUBLE;
//                // 1d. get optimal action for the current tensor
//                Action opt_action;
//                char opt_acidA, opt_acidB;
//                if (ins_score >= max(mm_score, del_score)) {
//                    opt_score = ins_score;
//                    opt_action = INSERTION;
//                    opt_acidA = acidA;
//                    opt_acidB = GAP_NOTATION;
//                } else if (del_score >= max(mm_score, ins_score)) {
//                    opt_score = del_score;
//                    opt_action = DELETION;
//                    opt_acidA = GAP_NOTATION;
//                    opt_acidB = acidB;
//                } else if (mm_score >= max(ins_score, del_score)) {
//                    opt_score = mm_score;
//                    opt_action = isMatch2(acidA,acidB)?MATCH:MISMATCH;
//                    opt_acidA = acidA;
//                    opt_acidB = acidB;
//                }
//                // 1e. assign the optimal score/action to the cell
//                tensor[i][j][k].score = opt_score;
//                tensor[i][j][k].action = opt_action;
//                tensor[i][j][k].acidA = opt_acidA;
//                tensor[i][j][k].acidB = opt_acidB;
//                // 1f. keep track of the globally optimal cell
//                if (opt_score >= max_score) {
//                    max_score = opt_score;
//                    max_i = i;
//                    max_j = j;
//                    max_k = k;
//                }
//            }
//        }
//    }
//    // 2. trace back
//    cout << "max_i: " << max_i << ", max_j: " << max_j << endl;
//    if (max_i == 0 or max_j == 0) {
//        trace.push_back(tensor[max_i][max_j][max_k]);
//        return; 
//    }
//    int i,j;
//    for (i = max_i, j = max_j; i > 0 and j > 0; ) {
//        trace.insert(trace.begin(), tensor[i][j][k]);
//        switch (tensor[i][j][k].action) {
//            case MATCH: i--; j--; break;
//            case MISMATCH: i--; j--; break;
//            case INSERTION: j--; break;
//            case DELETION: i--; break;
//            case UNDEFINED: cerr << "uncatched action." << endl; break;
//        }
//    }
//    if (i == 0 and j == 0) return;
//    // special cases
//    else 
//        trace.insert(trace.begin(), tensor[1][1]);
//}
/*}}}*/

void convexMSA (SequenceSet& allSeqs, vector<Tensor4D>& W, vector<Tensor4D>& C) {

}

int main (int argn, char** argv) {
    // 1. usage
    if (argn < 2) {
        usage();
        exit(1);
    }

    // 2. input DNA sequence file
    SequenceSet allSeqs (0, Sequence());
    ifstream seq_file(argv[1]);
    string tmp_str;
    int numSeq = 0;
    while (getline(seq_file, tmp_str)) {
        Sequence tmp_seq (tmp_str.begin(), tmp_str.end());
        allSeqs.push_back(tmp_seq);
        ++ numSeq;
    }
    seq_file.close();
    cout << "#########################################################" << endl;
    cout << "ScoreMatch: " << MATCH_SCORE;
    cout << ", ScoreInsertion: " << INSERTION_SCORE;
    cout << ", ScoreDeletion: " << DELETION_SCORE;
    cout << ", ScoreMismatch: " << MISMATCH_SCORE << endl;
    for (int i = 0; i < numSeq; i ++) {
        cout << "Seq_" << i << ": ";
        for (int j = 0; j < allSeqs[i].size(); j ++) 
            cout << allSeqs[i][j];
        cout << endl;
    }
    vector<int> lenSeqs (numSeq, 0);
    for (int i = 0; i < numSeq; i ++) 
        lenSeqs[i] = allSeqs[i].size();

    // 3. relaxed convex program: ADMM-based algorithm
    // Trace trace (0, Cell());
    vector<Tensor4D> W;  // W_1 .. W_n for each sequence
    vector<Tensor4D> C;  // Score for each W_1 .. W_n
    convexMSA (allSeqs, W, C);

    // 4. output the result
    /*
       cout << ">>>>>>>>>>>>>>>>>>>>>>>Summary<<<<<<<<<<<<<<<<<<<<<<<<" << endl;
       cout << "Length of Trace: " << trace.size();
       cout << ", Score: " << trace.back().score;
       cout << endl;
       int numInsertion = 0, numDeletion = 0, numMatch = 0, numMismatch = 0, numUndefined = 0;
       for (int i = 0; i < trace.size(); i ++) {
       switch (trace[i].action) {
       case MATCH: ++numMatch; break;
       case INSERTION: ++numInsertion; break;
       case DELETION: ++numDeletion; break;
       case MISMATCH: ++numMismatch; break;
       case UNDEFINED: ++numUndefined; break;
       }
       }
       cout << "numMatch: " << numMatch;
       cout << ", numInsertion: " << numInsertion;
       cout << ", numDeletion: " << numDeletion;
       cout << ", numMismatch: " << numMismatch;
       cout << ", numUndefined: " << numUndefined;
       cout << endl;
    // a. tuple view
    cout << ">>>>>>>>>>>>>>>>>>>>>>>TupleView<<<<<<<<<<<<<<<<<<<<<<<<" << endl;
    for (int i = 0; i < trace.size(); i ++) 
    cout << trace[i].toString() << endl;
    // b. sequence view
    cout << ">>>>>>>>>>>>>>>>>>>>>>>SequenceView<<<<<<<<<<<<<<<<<<<<<<<<" << endl;
    cout << "1st_aligned_DNA: ";
    for (int i = 0; i < trace.size(); i ++) 
    cout << trace[i].acidA;
    cout << endl;
    cout << "2nd_aligned_DNA: ";
    for (int i = 0; i < trace.size(); i ++) 
    cout << trace[i].acidB;
    cout << endl;
    */
    cout << "#########################################################" << endl;
    return 0;
}
