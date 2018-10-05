/*This is the c code for the counterfactual simulations in the callback paper draft. There are four main events that occur in the simulation
and a fifth event for policy 3 (FG):

    -Arrival Schedule: Determines the number of arrivals in each period and schedules queue check (if the policy is 3 (FG)).

    -Arrival: A new caller arrives. Because the rules for handling arrivals vary by policy, there is a seperate procedure embedded within the arrival
    subroutine for each policy.

    -Departure: A server becomes available after finishing service with a caller. Because the rules for handling departures are policy specific, there
    is a seperate procedure embedded within the arrival subroutine for each policy.

    -Abandon Decision: Every period in the simulation, all callers who are waiting in the online queue decide whether to abandon or wait until the end
    of the following period.

    -Queue Check: In policy 3 (FG), every period we check if there are available servers. If so, we check whether the offline queue should be receiving
    priority. If so, we initiate a callback to the offline queue.

The simulation code allows you to run and record the results of several number of iterations, servers, policies, and guarantee utility multipliers at
once. All of these choices are below under SIMULATION PARAMETERS.*/


#include "simlib.h"             /* Required for use of simlib.c. */
#include "assert.h"
#include "math.h"

#define EVENT_ARRIVAL          1  /* Event type for arrival of customer. */
#define EVENT_DEPARTURE        2  /* Event type for departure of customer after receiving service. */
#define EVENT_ABANDON_DECISION 3  /* Event type for abandonment decision of customers. */

#define LIST_ONLINE_QUEUE     1  /* List number for online queue. */
#define LIST_OFFLINE_QUEUE    2  /* List number for offline queue. */

#define STREAM                1  /* Random-number stream*/

#define max_cdf_size       598 /* This is the maximum number of entries in a cdf.*/
#define T_max              450 /* This is the maximum number of periods callers believe they will wait in the online queue before receiving service*/
#define Max_Wait_Minutes 75 /*Maximum number of minutes you can wait*/
#define n_message_subsets  180  /*Number of message subsets allowed (not including the no message subset).*/
#define max_servers        152 /*Set this to the maximum number of servers allowed in the simulation. This is used only for allocating space for variables.*/
#define N_Callers          10000000 /*Number of customers*/
#define N_Latent_Classes   2 /*Number of Latent Classes in the Model*/
#define N_Policies         5 /*Number of policies to test*/

/*SIMULATION PARAMETERS. THESE ARE WHAT WE TOGGLE WITH TO CREATE THE SIMULATION OUTPUT*/
/*Choose number of customers and burn-in period*/
#define Number_of_customers_required 120000 /*2431552*/ /*This is the total number of customers we run through each iteration of the simulation.*/
#define transient          20000 /*1215776*/ /* However, for gathering statistics we do not consider the calls where their num_custs_delayed is less than this*/

/*Choose number of iterations per policy number/agent number combination*/
#define n_iter             2 /* This is the number of times to iterate through the simulation. After each iterations /pi(t) and V(t) is updated based on previous service probabilities*/

/*Choose how many servers to test for each policy number*/
#define lowest_n_servers   50 /*This is the lowest number of servers you want to test in the simulations*/
#define highest_n_servers  50 /*This is the highest number of servers you want to test in the simulations*/
#define server_jump        4   /*When you loop through the number of servers, this is how many servers you want to jump by*/

/*Now come the policy choices, where policy 1 is policy N (No Callbacks), 2 is policy SQ (Status Quo), 3 is policy FG (Fixed Guarantee) and 4 is policy W (Window)*/
/*Choose which policies to simulate*/
#define lowest_policy_number 1 /* This is the lowest policy number you want to test in the simulations.*/
#define highest_policy_number 1 /* This is the highest policy number you want to test in the simulations.*/

/*Policy parameters for policy 4 (Window policy)*/
#define MID  30 /*Just for reference, the middle point of LB and UB*/
#define LB   20 /* This is the lower bound for the callback window in policy W*/
#define UB   40 /* This is the upper bound for the callback window in policy W*/

/*END SIMULATION PARAMETERS*/

/* Declare non-simlib global variables. */
int   iteration_count;
int   i, j, k, l, i_cdf, j_cdf, output_cdf, best_server, depart_server, current_period, aban_loop, num_custs_delayed, num_delays_required, num_server, n_servers, server_status[1+max_servers];
int   cdf_size[1+1], arr_serv_no, arrivalnum, iter, delay;
int   policy_number;
int   queue_to_serve;
int   period_length, periods_per_minute;
float online_wait_prediction;
float cdf[1+1][1+max_cdf_size], cdf_value;
float queue_length_online, queue_length_offline;
float temp, temp_time;
float v0, v1, v2;
float atrisk[1+n_message_subsets][1+2][1+T_max], servicenum[1+n_message_subsets][1+2][1+T_max], pt[1+n_message_subsets][1+2][1+T_max]; /*1 is online, 2 is offline*/
float req_wait_cdf[1+n_message_subsets][1+2][1+T_max], req_wait_pdf[1+n_message_subsets][1+2][1+T_max], EW[1+n_message_subsets][1+2][1+T_max]; /*1 is online, 2 is offline*/
float server_intime[1+max_servers], server_outtime[1+max_servers], server_util[1+max_servers], server_util_sum, server_util_total, server_busy_time[1+max_servers];
float cb_answer_prob[1+n_message_subsets][2]; /*0 is day and 1 is evening*/
float avg_service_time;
float abandon_prob;
float still_looping;
float starttime, wait_time[1+2], wait_time_total, AWT[1+2], AWT_total, calls_received[1+2], calls_received_total,
calls_answered[1+2], calls_answered_total,calls_abandoned, callbacks_not_answered, calls_not_serviced, abandon_rate,
callback_not_answer_rate, no_service_rate, avg_queue_length_total,avg_queue_length[3], callbacks_offered, callbacks_accepted,
percent_accept_callback, percent_answer_callback, AWT_All, Throughput, Rho_On, Rho_All;
float last_online_wait_time;
float LB_periods, UB_periods;
int Evening; /*Indicator that call is in evening*/
int Online_Message_Index[1+Max_Wait_Minutes][1+N_Policies]; /*This looks up what online message will be given to the caller given the predicted online wait and policy number.*/
int Offline_Message_Index[1+Max_Wait_Minutes][1+N_Policies]; /*This looks up what offline message will be given to the caller given the predicted online wait and policy number.*/
int Callback_Type_Index[1+Max_Wait_Minutes][1+N_Policies]; /*This looks up what callback type will be given to the caller given the predicted online wait and policy number.*/
/*0 = No callback, 1 = Scheduled (Alarm), 2 = Hold Spot in line, 3 = Window*/
int LB_Index[1+Max_Wait_Minutes][1+N_Policies]; /*This looks up what the LB (in minutes) is of the window that will be given to the caller given the predicted online wait.*/
/*This only matters if the offer type is 3 (Window)*/
int UB_Index[1+Max_Wait_Minutes][1+N_Policies]; /*This looks up what the UB (in minutes) is of the window that will be given to the caller given the predicted online wait.*/
/*This only matters if the offer type is 3 (Window)*/
int online_message, offline_message, callback_type, low_bound, up_bound, floor_online_wait_prediction, message;
int decision;
int caller_number;
float online_call_arrival_period, offline_call_arrival_period, scheduled_alarm_time;
int offline_message_minute;
int next_arrival_period;
int test;

/*Arrival Class*/
int Latent_Class[1+N_Callers], caller_class, caller_class_calc;

/*Arrival Rates*/
float Avg_Interstring_Time[1+N_Latent_Classes];

/*Posterior Segment Membership Probabilities*/
float Post_Prob[1+N_Callers][1+N_Latent_Classes]; /*We track the posterior probability of callers belonging to each segment every time a caller initiates a new call string or makes a decision*/
float sum_post_prob;

/*Model Parameters*/
float p_s[1+N_Latent_Classes], lambda_s[1+N_Latent_Classes], r[1+N_Latent_Classes][2], c_n[1+N_Latent_Classes][2], c_f[1+N_Latent_Classes][2], offline_pref[1+N_Latent_Classes][2];

/*Availability Probabilities*/
float avail_prob[1+Max_Wait_Minutes][2];

FILE  *infile, *outfile;

/* Declare non-simlib functions. */
void init_model(void); /*The subroutine for initializing the model*/
void arrive(void);     /*The subroutine for arrival of new customer.*/
void depart(int depart_server); /*The subroutine for departure of serviced customer*/
void abandon_decision(void); /*The subroutine for determining whether customers abandoned in the period*/
void record(void); /*The subroutine for recording the statistics into .csv file*/
int  empric_cdf(float cdf_value, int arr_sev_no); /*The subroutine for drawing value for empirical distribution*/

/*******************************************************************************************/

int main()  /* Main function. */
{
    /*Set Evening Indicator*/
    Evening = 0;

    /*Set still_looping to 1*/
    still_looping = 1;

    /*Average service time in minutes. Used for queue length based estimator in SQ policies.*/
    avg_service_time = 6.039893468;

    /*Fill in period length and calculate periods per minute*/
    period_length = 10; /*Period length in seconds*/
    periods_per_minute = 60/period_length; /*Number of decision periods in a minute.*/

    /*BEGIN BLOCK*/
    /*Fill out the policies indexes*/

    /*Start with policy 1 - No callback offered*/
    /*ONLINE MESSAGE INDEX*/
    for (i=5; i<=Max_Wait_Minutes; ++i){
        Online_Message_Index[i][1] = i; /*Greater than 4 they are given a delay announcement*/
    }

    /*Now do policy 2 - Status Quo*/
    /*ONLINE MESSAGE INDEX*/
    for (i=5; i<=59; ++i){
        Online_Message_Index[i][2] = i; /*Between 5 and 59 they are given a delay announcement*/
    }

    /*OFFLINE MESSAGE INDEX*/
    for (i=5; i<=59; ++i){
        Offline_Message_Index[i][2] = i; /*Between 5 and 59 they are given a callback offer*/
    }

    /*CALLBACK TYPE INDEX*/
    for (i=5; i<=59; ++i){
        Callback_Type_Index[i][2] = 1; /*Callback Scheduled*/
    }

     /*Now do policy 3 - Status Quo (Hold Spot)*/
    /*ONLINE MESSAGE INDEX*/
    for (i=5; i<=59; ++i){
        Online_Message_Index[i][3] = i; /*Between 5 and 59 they are given a delay announcement*/
    }

    /*OFFLINE MESSAGE INDEX*/
    for (i=5; i<=59; ++i){
        Offline_Message_Index[i][3] = i; /*Between 5 and 59 they are given a callback offer*/
    }

    /*CALLBACK TYPE INDEX*/
    for (i=5; i<=59; ++i){
        Callback_Type_Index[i][3] = 2; /*Spot Held*/
    }

    /*Now do policy 4 - Hold Spot, Always Offer Callback*/

    /*ONLINE MESSAGE INDEX*/
    for (i=0; i<=Max_Wait_Minutes; ++i){
        Online_Message_Index[i][4] = i; /*Always give delay announcement*/
    }

    /*OFFLINE MESSAGE INDEX*/
    for (i=0; i<=Max_Wait_Minutes; ++i){
        Offline_Message_Index[i][4] = i; /*Always give callback offer*/
    }

    /*CALLBACK TYPE INDEX*/
    for (i=0; i<=Max_Wait_Minutes; ++i){
        Callback_Type_Index[i][4] = 2; /*Always hold spot in line*/
    }

     /*Now do policy 5 - Window*/

    /*ONLINE MESSAGE INDEX*/
    for (i=0; i<=Max_Wait_Minutes; ++i){
        Online_Message_Index[i][5] = i; /*Always give delay announcement*/
    }

    /*OFFLINE MESSAGE INDEX*/
    for (i=0; i<=Max_Wait_Minutes; ++i){
        Offline_Message_Index[i][5] = MID; /*Always give callback offer with midpoint of 30 minutes*/
    }

    /*CALLBACK TYPE INDEX*/
    for (i=0; i<=Max_Wait_Minutes; ++i){
        Callback_Type_Index[i][5] = 3; /*Always offer callback window*/
    }

    /*LB INDEX*/
    for (i=0; i<=Max_Wait_Minutes; ++i){
        LB_Index[i][5] = LB; /*Always offer callback window*/
    }

     /*UB INDEX*/
    for (i=0; i<=Max_Wait_Minutes; ++i){
        UB_Index[i][5] = UB; /*Always offer callback window*/
    }

    /*Input parameter values*/
    p_s[1] = 0.972281853, p_s[2] = 0.027718147, lambda_s[1] = 0.000396765; lambda_s[2] = 0.025974819, r[1][0] = 5.783027604, r[1][1] = 6.20322235, r[2][0] = 5.477565075, r[2][1] = 5.809672146,
    c_n[1][0] = 0.017017419, c_n[1][1] = 0.010867923, c_n[2][0] = 0.01856109, c_n[2][1] = 0.009569585, c_f[1][0] = 0.003575103, c_f[1][1] = 0.002810464, c_f[2][0] = 0.005728517, c_f[2][1] = 0.002001847,
    offline_pref[1][0] = -1.200647571, offline_pref[1][1] = -0.877221376, offline_pref[2][0] = -0.777489296, offline_pref[2][1] = -0.627915692;

    /*Input availability probabilities*/
    avail_prob[1][0]=0.92845048648516, 	avail_prob[2][0]=0.92845048648516, 	avail_prob[3][0]=0.904322486644002, 	avail_prob[4][0]=0.920465217077223, 	avail_prob[5][0]=0.923540213000367, 	avail_prob[6][0]=0.925319546172627, 	avail_prob[7][0]=0.928084029747757, 	avail_prob[8][0]=0.931666666666667, 	avail_prob[9][0]=0.927807486631016, 	avail_prob[10][0]=0.933756890591423, 	avail_prob[11][0]=0.937177733347364, 	avail_prob[12][0]=0.926475871153155, 	avail_prob[13][0]=0.929320817228051, 	avail_prob[14][0]=0.931859730762839, 	avail_prob[15][0]=0.931657355679702, 	avail_prob[16][0]=0.931808644565674, 	avail_prob[17][0]=0.935081148564295, 	avail_prob[18][0]=0.92779888921368, 	avail_prob[19][0]=0.933917488134356, 	avail_prob[20][0]=0.933113129644922, 	avail_prob[21][0]=0.930668016194332, 	avail_prob[22][0]=0.919109663409338, 	avail_prob[23][0]=0.918280871670702, 	avail_prob[24][0]=0.924231593995711, 	avail_prob[25][0]=0.931122448979592, 	avail_prob[26][0]=0.932075471698113, 	avail_prob[27][0]=0.945220193340494, 	avail_prob[28][0]=0.936085219707057, 	avail_prob[29][0]=0.947284345047923, 	avail_prob[30][0]=0.933333333333333, 	avail_prob[31][0]=0.917089678510998, 	avail_prob[32][0]=0.931392931392931, 	avail_prob[33][0]=0.92, 	avail_prob[34][0]=0.924944812362031, 	avail_prob[35][0]=0.93029490616622, 	avail_prob[36][0]=0.941176470588235, 	avail_prob[37][0]=0.938983050847458, 	avail_prob[38][0]=0.929824561403509, 	avail_prob[39][0]=0.929752066115703, 	avail_prob[40][0]=0.956310679611651, 	avail_prob[41][0]=0.921658986175115, 	avail_prob[42][0]=0.950495049504951, 	avail_prob[43][0]=0.901098901098901, 	avail_prob[44][0]=0.941935483870968, 	avail_prob[45][0]=0.92156862745098, 	avail_prob[46][0]=0.898305084745763, 	avail_prob[47][0]=0.909090909090909, 	avail_prob[48][0]=0.951219512195122, 	avail_prob[49][0]=0.91588785046729, 	avail_prob[50][0]=0.913461538461538, 	avail_prob[51][0]=0.952941176470588, 	avail_prob[52][0]=0.974358974358974, 	avail_prob[53][0]=0.867647058823529, 	avail_prob[54][0]=0.971014492753623, 	avail_prob[55][0]=0.966101694915254, 	avail_prob[56][0]=0.957142857142857, 	avail_prob[57][0]=0.927272727272727, 	avail_prob[58][0]=0.864864864864865, 	avail_prob[59][0]=0.96875, 	avail_prob[60][0]=0.636363636363636, 	avail_prob[61][0]=0.875, 	avail_prob[62][0]=0.888888888888889, 	avail_prob[63][0]=1, 	avail_prob[64][0]=1, 	avail_prob[65][0]=1, 	avail_prob[66][0]=1, 	avail_prob[67][0]=1, 	avail_prob[68][0]=1, 	avail_prob[69][0]=1, 	avail_prob[70][0]=0.5, 	avail_prob[71][0]=1, 	avail_prob[72][0]=1, 	avail_prob[73][0]=1, 	avail_prob[74][0]=1, 	avail_prob[75][0]=1;
    avail_prob[1][1]=0.933064875582128, 	avail_prob[2][1]=0.933064875582128, 	avail_prob[3][1]=0.917647058823529, 	avail_prob[4][1]=0.905806451612903, 	avail_prob[5][1]=0.930602957906712, 	avail_prob[6][1]=0.932584269662921, 	avail_prob[7][1]=0.936858721389108, 	avail_prob[8][1]=0.938760806916427, 	avail_prob[9][1]=0.934244791666667, 	avail_prob[10][1]=0.944036178631996, 	avail_prob[11][1]=0.947206560738083, 	avail_prob[12][1]=0.939990162321692, 	avail_prob[13][1]=0.93740972556572, 	avail_prob[14][1]=0.932987258140632, 	avail_prob[15][1]=0.933551708001872, 	avail_prob[16][1]=0.943421643466547, 	avail_prob[17][1]=0.939557961208841, 	avail_prob[18][1]=0.944870565675935, 	avail_prob[19][1]=0.933145009416196, 	avail_prob[20][1]=0.947761194029851, 	avail_prob[21][1]=0.931989924433249, 	avail_prob[22][1]=0.938378378378378, 	avail_prob[23][1]=0.938695163104612, 	avail_prob[24][1]=0.932546374367622, 	avail_prob[25][1]=0.932632880098888, 	avail_prob[26][1]=0.92667898952557, 	avail_prob[27][1]=0.919741100323625, 	avail_prob[28][1]=0.929752066115703, 	avail_prob[29][1]=0.933726067746686, 	avail_prob[30][1]=0.926076360682372, 	avail_prob[31][1]=0.93015332197615, 	avail_prob[32][1]=0.930491195551437, 	avail_prob[33][1]=0.917657822506862, 	avail_prob[34][1]=0.933399602385686, 	avail_prob[35][1]=0.929012345679012, 	avail_prob[36][1]=0.929844097995546, 	avail_prob[37][1]=0.93208430913349, 	avail_prob[38][1]=0.906976744186047, 	avail_prob[39][1]=0.933414043583535, 	avail_prob[40][1]=0.940939597315436, 	avail_prob[41][1]=0.931623931623932, 	avail_prob[42][1]=0.929712460063898, 	avail_prob[43][1]=0.934189406099519, 	avail_prob[44][1]=0.927536231884058, 	avail_prob[45][1]=0.929982046678636, 	avail_prob[46][1]=0.936416184971098, 	avail_prob[47][1]=0.929824561403509, 	avail_prob[48][1]=0.948979591836735, 	avail_prob[49][1]=0.934977578475336, 	avail_prob[50][1]=0.940265486725664, 	avail_prob[51][1]=0.915816326530612, 	avail_prob[52][1]=0.920096852300242, 	avail_prob[53][1]=0.91740412979351, 	avail_prob[54][1]=0.921511627906977, 	avail_prob[55][1]=0.927027027027027, 	avail_prob[56][1]=0.916129032258065, 	avail_prob[57][1]=0.92485549132948, 	avail_prob[58][1]=0.947019867549669, 	avail_prob[59][1]=0.924731182795699, 	avail_prob[60][1]=0.865771812080537, 	avail_prob[61][1]=0.926315789473684, 	avail_prob[62][1]=0.913461538461538, 	avail_prob[63][1]=0.87012987012987, 	avail_prob[64][1]=0.8625, 	avail_prob[65][1]=0.942307692307692, 	avail_prob[66][1]=0.791666666666667, 	avail_prob[67][1]=0.959183673469388, 	avail_prob[68][1]=0.840909090909091, 	avail_prob[69][1]=0.78125, 	avail_prob[70][1]=0.852941176470588, 	avail_prob[71][1]=0.833333333333333, 	avail_prob[72][1]=0.782608695652174, 	avail_prob[73][1]=0.9, 	avail_prob[74][1]=0.666666666666667, 	avail_prob[75][1]=0.5;


    /*Set Latent Classes*/
    for (i=1; i<=9722818; ++i){
        Latent_Class[i] = 1;
    }

    for (i=9722819; i<=N_Callers; ++i){
        Latent_Class[i] = 2;
    }

    /*Set Average Time Between Strings in Epochs*/
    Avg_Interstring_Time[1]=(1/lambda_s[1])*24*60*60/period_length;
    Avg_Interstring_Time[2]=(1/lambda_s[2])*24*60*60/period_length;

    /*Read in cdf size*/
    cdf_size[1] = 598;

    /*Open input file. The input file contains the parameters from the callback model, the size of the cdfs,
    and the entries of two cdfs: 1) the poisson distribution of the number of calls to arrive in a period
    2) the empirical cdf of service times.*/
    infile  = fopen("Inputs.in",  "r");
    if (infile == NULL) {
        fprintf(stderr, "Could not open Inputs.in.");
        exit(1);
    }

	/*This array reads the cdf from the data file.*/
    for (i=1; i<=cdf_size[1]; ++i){ /*Read in each each entry of the cdf*/
        fscanf(infile,"%f", &cdf[1][i]);
    }

    /*Set iteration count for counting number of simulations we've run*/
    iteration_count=0;

    /*Open output file, which is a .csv where we collect the simulation statistics*/
	outfile = fopen("Simulation Statistics.csv", "w");
    fprintf(outfile,"Iteration, Servers, Policy, Throughput, AWT_All, AWT_On, Rho_On, Rho_All, AWT(Online),AWT(Offline),AWT(All),");
    fprintf(outfile,"CALLS_RECEIVED(Online),CALLS_RECEIVED(Offline),CALLS_RECEIVED(All),CALLS_ANSWERED(Online),CALLS_ANSWERED(Offline),");
    fprintf(outfile,"CALLS_ANSWERED(All),CALLS_ABANDONED(Online),CALLBACKS_NOT_ANSWERED(Offline),CALLS_NOT_SERVICED(All),ABANDON_RATE(Online),");
    fprintf(outfile,"CALLBACK_NOT_ANSWER_RATE(Offline),NO_SERVICE_RATE(All),AVG_QUEUE_LENGTH(Online),AVG_QUEUE_LENGTH(Offline),AVG_QUEUE_LENGTH(All),");
    fprintf(outfile,"SERVER_UTILIZATION,Sim_Time,Percent_Accepting_Callback,Percent_Answering_Callback\n");


    /*We iterate through different number of servers in the system*/
    for (n_servers=lowest_n_servers; n_servers<=highest_n_servers; n_servers = n_servers + server_jump){

        /*Reset the random numbers*/
        lcgrandst(1973272912,1);

    /*We iterate through the different policies*/
    for (policy_number=lowest_policy_number; policy_number<=highest_policy_number; ++policy_number){

        /*Reset the service probabilities to be a vector of zeros*/
        for (i=0; i<=n_message_subsets; ++i){
            for (j=1; j<=2; ++j){
                for (k=1; k<=T_max; ++k){
                    servicenum[i][j][k]=0;
                    atrisk[i][j][k]=0;
                    req_wait_cdf[i][j][k]=0;
                    req_wait_pdf[i][j][k]=0;
                    EW[i][j][k]=0;
                }
            }
        }

        /*Reset the cb_answer_probabilities to be a vector of zeros*/
        for (i=0; i<=n_message_subsets; ++i){
            for (j=0; j<=1; ++j){
                cb_answer_prob[i][j]=0;
            }
        }

        /*Reset the random numbers*/
        lcgrandst(1973272912,1);

    /*We iterate through the predetermined number of iterations.*/
    for (iter=1; iter<=n_iter; ++iter){

        printf("Servers = %d, Policy = %d, Iteration %d\n",n_servers,policy_number,iter); /*Print which iteration we're on*/

        /*At the beginning of each iteration we update the service probabilities (pt) and value function using the empirical distribution of service probabilities
        from the previous iteration. For the first iteration, we assume all service probabilities are zero.*/

        /*BEGIN BLOCK*/
        /*In this block we update the service probabilities pt for each message subset.*/
        if (iter==1){
            for (i=0; i<=n_message_subsets; ++i){
                for (j=1; j<=2; ++j){ /*1 is online and 2 is offline*/
                    for (k=1; k<=T_max; ++k){
                        pt[i][j][k]=0; /*Message, queue, period*/
                    }
                    pt[i][j][T_max+1] = 1;
                }
            }
        }

        if (iter>=2){
            for (i=0; i<=n_message_subsets; ++i){
                for (j=1; j<=2; ++j){ /*1 is online and 2 is offline*/
                    for (k=1; k<=T_max; ++k){
                        if (servicenum[i][j][k]>0){
                            pt[i][j][k]=servicenum[i][j][k]/atrisk[i][j][k];
                        }else{
                            pt[i][j][k]=0;
                        }
                    }
                    pt[i][j][T_max+1]=1;
                }
            }

            /*Clear out the servicenum and atrisk trackers*/
            for (i=0; i<=n_message_subsets; ++i){
                for (j=1; j<=2; ++j){ /*1 is online and 2 is offline*/
                    for (k=1; k<=T_max; ++k){
                        servicenum[i][j][k]=0;
                        atrisk[i][j][k]=0;
                        req_wait_cdf[i][j][k]=0;
                        req_wait_pdf[i][j][k]=0;
                        EW[i][j][k]=0;
                    }
                }
            }
        }


        /*Reset the cb_answer_probabilities to be a vector of zeros*/
        for (i=0; i<=n_message_subsets; ++i){
            for (j=0; j<=1; ++j){
                cb_answer_prob[i][j]=0;
            }
        }

        /*END BLOCK*/

        /*BEGIN BLOCK*/
        /*In this block we find the expected waiting times given the message, the period, and the channel.*/

        /*FIRST, REQUIRED WAITING TIME CDF*/
        for (i=0; i<=n_message_subsets; ++i){
            for (j=1; j<=2; ++j){
                req_wait_cdf[i][j][1] = pt[i][j][1];
            }
        }

        for (i=0; i<=n_message_subsets; ++i){
            for (j=1; j<=2; ++j){
                for (k=2; k<=T_max; ++k){
                    req_wait_cdf[i][j][k] = req_wait_cdf[i][j][k-1]+(1-req_wait_cdf[i][j][k-1])*pt[i][j][k];
                }
            }
        }


        /*SECOND, REQUIRED WAITING TIME PDF*/
        for (i=0; i<=n_message_subsets; ++i){
            for (j=1; j<=2; ++j){
                req_wait_pdf[i][j][1] = req_wait_cdf[i][j][1];
            }
        }

        for (i=0; i<=n_message_subsets; ++i){
            for (j=1; j<=2; ++j){
                for (k=2; k<=T_max; ++k){
                    req_wait_pdf[i][j][k] = req_wait_cdf[i][j][k]-req_wait_cdf[i][j][k-1];
                }
            }
        }

        /*THIRD, EXPECTED WAITING TIME*/
        for (i=0; i<=n_message_subsets; ++i){
            for (j=1; j<=2; ++j){
                for (l=1; l<=T_max; ++l){
                    EW[i][j][1] = EW[i][j][1] + l*req_wait_pdf[i][j][l];
                }
            }
        }

        for (i=0; i<=n_message_subsets; ++i){
            for (j=1; j<=2; ++j){
                for (k=2; k<=T_max; ++k){
                    for (l=k; l<=T_max; ++l){
                        if(req_wait_cdf[i][j][l]<1){
                            EW[i][j][k] = EW[i][j][k] + (l-k+1)*req_wait_pdf[i][j][l]/(1-req_wait_cdf[i][j][k-1]);
                        }else{
                            EW[i][j][k] = EW[i][j][k] + 0;
                        }
                    }
                }
            }
        }

        /*BEGIN BLOCK*/
        /*In this block we find the callback answering probabilities by message by day/evening given the availability probabilities and
        waiting time distribution in the offline queue for each offline message*/
        for (i=0; i<=n_message_subsets; ++i){
            for (j=0; j<=1; ++j){ /*Day and Evening*/
                for (k=1; k<=T_max; ++k){
                    offline_message_minute = ceil(k/periods_per_minute);
                    cb_answer_prob[i][j]=cb_answer_prob[i][j]+req_wait_pdf[i][2][k]*avail_prob[offline_message_minute][j];
                }
            }
        }

        /*Number of times we've done a simulation*/
        ++iteration_count;

        printf("%d\n",iteration_count);

        if(iteration_count>=2){
            cleanup_simlib(); /*This function is needed to clear out previous data when initializing simlib*/
        }

        /* Initialize simlib */
        init_simlib();

        /* Set maxatr = max(maximum number of attributes per record, 4) */
        maxatr = 10;  /* NEVER SET maxatr TO BE SMALLER THAN 4. */ /*BACK*/

        /*In policy SQ, we want the offline callers placed in queue in order of their expected callback time.
        We use transfer[9] to record their expected callback time at the time of their offer.*/
        list_rank[LIST_OFFLINE_QUEUE] = 9;

        /* Initialize the model. */
        init_model();

        /* Run the simulation until reaching the required number of customers. */
        while (num_custs_delayed < Number_of_customers_required) {

            /* Determine the next event. */
            timing();

            switch (next_event_type) {

                case EVENT_ARRIVAL:
                    arrive();
                    break;

                case EVENT_DEPARTURE:
                    depart( (int) transfer[3] );
                    break;

                case EVENT_ABANDON_DECISION:
                    abandon_decision();
                    break;
            }
        }

    record(); /*Record statistics in the .csv file.*/
    } /*Closing the loop for guarantee utility multiplier*/
    } /*Closing the loop for iter*/
    } /*Closing the loop for policy_number*/
    fclose(infile);
    fclose(outfile);

    return 0;
}

/*******************************************************************************************/

void init_model(void)  /* Initialization function. */
{
	/*Making all servers idle and resetting their statistics.*/
	for (i=1; i<=n_servers; ++i){
		server_status[i]=0;
		server_intime[i]=0.0;
		server_outtime[i]=0.0;
		server_util[i]=0.0;
		server_busy_time[i]=0.0;
	}

	/*Reset statistical counters*/
	starttime=0;
	wait_time[1]=0;
	wait_time[2]=0;
	calls_received[1]=0;
	calls_received[2]=0;
    calls_answered[1]=0;
    calls_answered[2]=0;
    calls_abandoned = 0;
    callbacks_not_answered = 0;
    callbacks_offered = 0;
    callbacks_accepted = 0;

   	num_custs_delayed = 0; /*Reset the number of customers delayed*/

    /*Setting initial posterior probabilities*/
   	for (i=1; i<=N_Callers; ++i){
        for (j=1; j<=N_Latent_Classes; ++j){
            Post_Prob[i][j]=p_s[j];
        }
   	}

	/*Scheduling arrival events*/
	for (i=1; i<=N_Callers; ++i){
        transfer[10]=i; /*Record caller number in transfer array for retrieval later*/
        next_arrival_period = ceil(expon(Avg_Interstring_Time[Latent_Class[i]],STREAM)); /*When the caller will arrive*/
        event_schedule(next_arrival_period,EVENT_ARRIVAL);

        /*Update Posterior probabilities*/
        sum_post_prob = 0;
        for (j=1; j<=N_Latent_Classes; ++j){
            Post_Prob[i][j]=Post_Prob[i][j]*lambda_s[j]*exp(-lambda_s[j]*(next_arrival_period/24/60/60*period_length));
            sum_post_prob = sum_post_prob + Post_Prob[i][j];
        }
        for (j=1; j<=N_Latent_Classes; ++j){
            Post_Prob[i][j]=Post_Prob[i][j]/sum_post_prob;
            if (Post_Prob[i][j]>1-.0000000001){
                Post_Prob[i][j]=1;
            }
            if (Post_Prob[i][j]<.0000000001){
                Post_Prob[i][j]=0;
            }
        }
	}

	/*Scheduling the initial abandonment decision event.*/
	event_schedule(sim_time+0.01,EVENT_ABANDON_DECISION);
}

/*******************************************************************************************/

void arrive(void)  /* Arrival event function. */
{
    /*Update caller number and latent class*/
    caller_number = transfer[10];
    caller_class = Latent_Class[caller_number];

    /*Reset best server and temp_time*/
    best_server=0;
    temp_time=0;

    /*Pick the server that has been idle for the longest time*/
    for (num_server=n_servers; num_server>=1; --num_server){
        if (((sim_time-server_busy_time[num_server])>=temp_time) && (server_status[num_server]==0)){
            best_server=num_server;
            temp_time=sim_time-server_busy_time[num_server];
        }
    }

    if (best_server>0) { /*There is an idle server. So, the caller is immediately served*/

        /* Record the time of arrival to the server and make server busy*/
        server_intime[best_server]=sim_time;
        server_status[best_server]=1;

        /*Update statistics*/
        if (num_custs_delayed>=transient-1){
            ++calls_received[1];
            ++calls_answered[1];
        }

        /*Increment num_custs_delayed and do record starttime if the transient threshold has been passed*/
        ++num_custs_delayed;
        if (num_custs_delayed==transient){
            starttime=sim_time;
        }

        /*Update last_online_wait_time for the next time we generate an expected wait in the online queue.*/
        last_online_wait_time = 0;

        /* Schedule a departure (service completion) for this server, and save the server number in attribute 3
         of the event list. */
        transfer[3]=best_server;
        temp=floor(sim_time)+empric_cdf(lcgrand(STREAM),1); /*Randomly draw service time from empirical distribution of service times*/
        event_schedule(temp, EVENT_DEPARTURE);

   }else{ /*There are no idle servers*/

        /*BEGIN BLOCK*/

        /*Generate the expected waiting time in the online queue, which is used to determine what messages are delivered to caller*/
        if (policy_number==1){ /*Status Quo Policy*/

            /*For policy SQ, it is a queue-length based estimator*/
            queue_length_online = list_size[LIST_ONLINE_QUEUE];
            queue_length_offline = list_size[LIST_OFFLINE_QUEUE];
            online_wait_prediction = avg_service_time*(queue_length_online + queue_length_offline)/n_servers;

        }else{ /*Rest of the policies*/

            /*For the rest of the policies, it is the most previous waiting time in the online queue*/
            online_wait_prediction = last_online_wait_time;
        }
        /*END BLOCK*/

        /*BEGIN BLOCK*/
        /*Generate the messages, which include the online_message, the callback_message, and the callback_type, where
        the types are the following: 0 = No callback offered, 1 = Alarm (Scheduled), 2 = Hold Spot, 3 = Window*,
        and if necessary the LB and UB for the window policy*/
        floor_online_wait_prediction = floor(online_wait_prediction);

        online_message = Online_Message_Index[floor_online_wait_prediction][policy_number];
        offline_message = Offline_Message_Index[floor_online_wait_prediction][policy_number];
        callback_type = Callback_Type_Index[floor_online_wait_prediction][policy_number];
        low_bound = LB_Index[floor_online_wait_prediction][policy_number];
        up_bound = UB_Index[floor_online_wait_prediction][policy_number];
        /*END BLOCK*/

        /* All servers are busy. So, if the policy dictates, we offer callback and caller chooses which queue to join. If policy does not dictate
        callback offer, then caller chooses whether to immediately abandon.*/

        /*Determine nominal utilities of actions.*/
        v0 = 0; /*Nominal utility of abandoning*/
        v1 = r[caller_class][Evening] - c_n[caller_class][Evening]*EW[online_message][1][1]; /*nominal utility of waiting in online queue*/
        v2 = offline_pref[caller_class][Evening] - c_f[caller_class][Evening]*EW[offline_message][2][1] + r[caller_class][Evening]*cb_answer_prob[offline_message][Evening]; /*Nominal utility of accepting callback offer*/

        if (callback_type==0){ /*No callback offered*/
            temp = lcgrand(STREAM);
            if(temp<exp(v0)/(exp(v0)+exp(v1))){
                decision = 0; /*Caller immediately abandons*/
            }else{
                decision = 1; /*Caller joins online queue*/
            }

        }else{ /*Callback is offered*/

            /*Update statistics*/
            if (num_custs_delayed>=transient-1){
                ++callbacks_offered;
            }

            temp = lcgrand(STREAM);
            if(temp<exp(v2)/(exp(v0)+exp(v1)+exp(v2))){ /*Caller accepts callback offer*/
                decision = 2;
            }else{
                temp = lcgrand(STREAM);
                if(temp<exp(v0)/(exp(v0)+exp(v1))){
                    decision = 0; /*Caller immediately abandons*/
                }else{
                    decision = 1; /*Caller joins online queue*/
                }
            }
        }


        /*CASE 1: CALLER IMMEDIATELY ABANDONS*/
        if (decision==0){

            /*Update Statistics*/
            if (num_custs_delayed>=transient-1){
                ++calls_received[1];
                ++calls_abandoned;
            }

			/*Increment num_custs_delayed and record starttime if the transient threshold has been passed*/
			++num_custs_delayed;
            if (num_custs_delayed==transient){
                starttime=sim_time;
            }

            /*Schedule next arrival for caller*/
            next_arrival_period = ceil(expon(Avg_Interstring_Time[caller_class],STREAM)); /*Generate from caller's arrival rate*/
            event_schedule(sim_time+next_arrival_period,EVENT_ARRIVAL);
        }


        /*UPDATE TRANSFER ARRAY FOR CASES 2 and 3*/
        transfer[1] = sim_time; /*We'll use this later to determine waiting times*/
        transfer[10] = caller_number;
        transfer[4] = online_message;
        transfer[5] = offline_message;
        transfer[6] = callback_type;
        transfer[7] = low_bound;
        transfer[8] = up_bound;
        transfer[9] = online_message * periods_per_minute + sim_time; /*This is when the caller is promised the callback. If the caller chooses to
        accept a callback, we use transfer[9] later to determine priority.*/

        /*CASE 2: CALLER JOINS ONLINE QUEUE*/
        if (decision==1){
            /*Place the caller at the end of the online queue*/
            list_file(LAST, LIST_ONLINE_QUEUE);
        }

        /*CASE 3: CALLER ACCEPTS CALLBACK OFFER*/
        if (decision==2){

            /*Update statistics*/
            if (num_custs_delayed>=transient-1){
                ++callbacks_accepted;
            }

            if (callback_type==1){ /*Scheduled Callback*/
                /*Place the caller in the offline queue sorted in increasing order of their expected time to receive their callback.*/
                list_file(INCREASING, LIST_OFFLINE_QUEUE);
            }else{
                /*Place the caller at the end of the offline queue*/
                list_file(LAST, LIST_OFFLINE_QUEUE);
            }
        }
   }
}

/*******************************************************************************************/

void depart(int depart_server)  /* Departure event function. */
{
    /*Update caller number and latent class*/
    caller_number = transfer[10];
    caller_class = Latent_Class[caller_number];

    /*Schedule next arrival for caller*/
    next_arrival_period = ceil(expon(Avg_Interstring_Time[caller_class],STREAM)); /*Generate from caller's arrival rate*/
    event_schedule(sim_time+next_arrival_period,EVENT_ARRIVAL);

    /*Update server statistics*/
    server_outtime[depart_server]=sim_time;
    server_busy_time[depart_server]=server_busy_time[depart_server]+server_outtime[depart_server]-server_intime[depart_server];

    /*BEGIN BLOCK*/
    /* In this block, we determine based on the policy which queue to serve. If the outcome is zero, we don't serve any queue. If it is 1, then we serve
    the online queue and if it is zero, we serve the offline queue. If we serve the offline queue, we determine whether the caller answers to arriving callback
    If the caller rejects the arriving callback, we loop through the block again. We continue looping until there is noone to serve, we get a caller from the
    online queue or we get a caller from the offline queue who answers the callback when it arrives.*/

    while (still_looping ==1){

        /*Case 1, both queues empty*/
        if (list_size[LIST_ONLINE_QUEUE]==0 && list_size[LIST_OFFLINE_QUEUE]==0){

            /*No queue to serve*/
            queue_to_serve = 0;
            break;
        }

        /*Case 2, online queue has caller and offline queue is empty*/
        if (list_size[LIST_ONLINE_QUEUE]>0 && list_size[LIST_OFFLINE_QUEUE]==0){

            /*Serve online queue*/
            queue_to_serve = 1;
            break;
        }

        /*Case 3, offline queue has caller*/
        if (list_size[LIST_OFFLINE_QUEUE]>0){

            /*Pick the caller at the end of the offline queue*/
            list_remove(FIRST, LIST_OFFLINE_QUEUE);

            /*Bring in callback_offer_type from transfer array*/
            callback_type = transfer[6];

            /*CASE 3.1 - SCHEDULED (ALARM)*/
            if (callback_type == 1){

                /*Recover the scheduled callback time and arrival time*/
                offline_call_arrival_period = transfer[1];
                scheduled_alarm_time = transfer[9];

                /*Previously, we stored the caller's expected time to receive the callback. If the simulation time is greater than or equal to
                that expected time, then the caller is offered a callback. If not, the caller is placed back into the offline queue.*/
                if(sim_time>=scheduled_alarm_time){ /*Callback is initiated*/

                    /*Generate random number for determining whether callback is answered*/
                    temp = lcgrand(STREAM);

                    offline_message_minute = ceil((sim_time-offline_call_arrival_period)/periods_per_minute);

                    if(temp<avail_prob[offline_message_minute][Evening]){ /*Caller is available to take callback*/

                        /*Serve offline queue*/
                        queue_to_serve = 2;

                        /*Put it back into the offline queue list as we will remove it later in the procedure*/
                        list_file(FIRST, LIST_OFFLINE_QUEUE);

                        /*Break out of while loop*/
                        break;

                    }else{ /*Caller is not available to take callback*/

                        /*Update statistics*/
                        if (num_custs_delayed>=transient-1){
                            wait_time[2]=wait_time[2]+sim_time - transfer[1];
                            ++calls_received[2];
                            ++callbacks_not_answered;
                        }

                        /*Increment num_custs_delayed and record starttime if the transient threshold has been passed*/
                        ++num_custs_delayed;
                        if (num_custs_delayed==transient){
                            starttime=sim_time;
                        }
                    }

                }else{ /*Caller at the end of the offline queue has not waited until the expected time that the callback will arrive. So, don't serve anyone.*/

                    if(list_size[LIST_ONLINE_QUEUE]>0){ /*There is a caller waiting in the online queue*/
                        queue_to_serve = 1;
                    }else{
                        queue_to_serve = 0;
                    }

                    /*Put the caller back into queue*/
                    list_file(FIRST, LIST_OFFLINE_QUEUE);

                    /*Break out of while loop*/
                    break;
                }
            }

            /*CASE 3.2 - HOLD SPOT*/
            if (callback_type == 2){

                if(list_size[LIST_ONLINE_QUEUE]>0){ /*There is a caller in online queue. So, we need to see if that caller has been waiting the longest.*/

                    /*Retrieve the period that the offline caller originally arrived in*/
                    offline_call_arrival_period = transfer[1];

                    /*Put the caller back into queue*/
                    list_file(FIRST, LIST_OFFLINE_QUEUE);

                   /*Pick the caller at the end of the online queue*/
                    list_remove(FIRST, LIST_ONLINE_QUEUE);

                    /*Retrieve the period that the online caller originally arrived in*/
                    online_call_arrival_period = transfer[1];

                    /*Put the caller back into queue*/
                    list_file(FIRST, LIST_ONLINE_QUEUE);

                    if(online_call_arrival_period<=offline_call_arrival_period){ /*The caller in the online queue has been waiting longer*/

                        /*Serve the online queue*/
                        queue_to_serve = 1;
                        break; /*Break out of while loop*/
                    }
                }

                /*If we didn't break out the while loop previously, that means we're initiating a callback*/

                /*Pick the caller at the end of the offline queue*/
                list_remove(FIRST, LIST_OFFLINE_QUEUE);

                /*Generate random number for determining whether callback is answered*/
                temp = lcgrand(STREAM);

                offline_message_minute = ceil((sim_time-offline_call_arrival_period)/periods_per_minute);

                if(temp<avail_prob[offline_message_minute][Evening]){ /*Caller is available to take callback*/

                    /*Serve offline queue*/
                    queue_to_serve = 2;

                    /*Put it back into the offline queue list as we will remove it later in the procedure*/
                    list_file(FIRST, LIST_OFFLINE_QUEUE);

                    /*Break out of while loop*/
                    break;

                }else{ /*Caller is not available to take callback*/

                    /*Update statistics*/
                    if (num_custs_delayed>=transient-1){
                        wait_time[2]=wait_time[2]+sim_time - transfer[1];
                        ++calls_received[2];
                        ++callbacks_not_answered;
                    }

                    /*Increment num_custs_delayed and record starttime if the transient threshold has been passed*/
                    ++num_custs_delayed;
                    if (num_custs_delayed==transient){
                        starttime=sim_time;
                    }
                }
            }

            /*CASE 3.3 - WINDOW*/
            if (callback_type == 3){

                /*Recover the lower bound and upper bound of the window and arrival time*/
                offline_call_arrival_period = transfer[1];
                low_bound = transfer[7];
                up_bound = transfer[8];

                /*We first determine if we need to serve the online queue, which we only do if there are callers
                in the online queue and the caller at the end of the offline queue hasn't waited past the window*/
                if(list_size[LIST_ONLINE_QUEUE]>0){ /*There is a caller in online queue*/

                    if(sim_time<offline_call_arrival_period + up_bound * periods_per_minute){ /*The offline call has not breached the window*/

                        /*Serve the online queue*/
                        queue_to_serve = 1;
                        break; /*Break out of while loop*/
                    }
                }

                /*If we didn't break out of the while loop previously, that means we're checking to see whether to initiate callback based on the
                lower bound of the window policy*/

                /*Previously, we stored the caller's lower bound for the window policy. If the simulation time is greater than or equal to
                that lower bound, then the caller is offered a callback. If not, the caller is placed back into the offline queue.*/
                if(sim_time>=offline_call_arrival_period + low_bound * periods_per_minute){ /*Callback is initiated*/

                    /*Generate random number for determining whether callback is answered*/
                    temp = lcgrand(STREAM);

                    offline_message_minute = ceil((sim_time-offline_call_arrival_period)/periods_per_minute);

                    if(temp<avail_prob[offline_message_minute][Evening]){ /*Caller is available to take callback*/

                        /*Serve offline queue*/
                        queue_to_serve = 2;

                        /*Put it back into the offline queue list as we will remove it later in the procedure*/
                        list_file(FIRST, LIST_OFFLINE_QUEUE);

                        /*Break out of while loop*/
                        break;

                    }else{ /*Caller is not available to take callback*/

                        /*Update statistics*/
                        if (num_custs_delayed>=transient-1){
                            wait_time[2]=wait_time[2]+sim_time - transfer[1];
                            ++calls_received[2];
                            ++callbacks_not_answered;
                        }

                        /*Increment num_custs_delayed and record starttime if the transient threshold has been passed*/
                        ++num_custs_delayed;
                        if (num_custs_delayed==transient){
                            starttime=sim_time;
                        }
                    }

                }else{ /*Caller at the end of the offline queue has not waited until the lower bound of the window policy. So, don't serve anyone.*/

                    /*No queue to serve*/
                    queue_to_serve = 0;

                    /*Put the caller back into queue*/
                    list_file(FIRST, LIST_OFFLINE_QUEUE);

                    /*Break out of while loop*/
                    break;
                }
            }
        }
    }
    /*END BLOCK*/

    if (queue_to_serve == 0){
        /* We aren't serving anyone. So, make server idle.*/
        server_status[depart_server]=0;

    }else{ /*We have a caller to serve*/

        /*Take the caller from the head of the queue that we just determined we should serve*/
        list_remove(FIRST,queue_to_serve);

        /*Update statistics*/
        if (num_custs_delayed>=transient-1){
            wait_time[queue_to_serve]=wait_time[queue_to_serve]+sim_time - transfer[1];
            ++calls_received[queue_to_serve];
            ++calls_answered[queue_to_serve];
        }

        /*Increment num_custs_delayed and record starttime if the transient threshold has been passed*/
        ++num_custs_delayed;
        if (num_custs_delayed==transient){
            starttime=sim_time;
        }

        /*Update last_online_wait_time for the next time we generate an expected wait in the online queue.*/
        if(queue_to_serve==1){
            last_online_wait_time = (sim_time-transfer[1])/6;
        }

        /*BEGIN BLOCK*/
        /*In this block, we add the waiting time of this answered call to a table for figuring out pt (the service probabilities at the beginning of the next iteration.*/

        /*How long did caller wait*/
        delay=floor(sim_time-transfer[1]);

        /*Determine the message*/
        if (queue_to_serve ==1){
            message = transfer[4];
        }else{
            message = transfer[5];
        }

        /*atrisk[1+n_message_subsets][1+2][1+T_max], servicenum[1+n_message_subsets][1+2][1+T_max];*/

        if (delay>0 && num_custs_delayed>=transient){
            servicenum[message][queue_to_serve][delay]=servicenum[message][queue_to_serve][delay]+1;
            for (i=1; i<=delay; ++i){
                atrisk[message][queue_to_serve][i]=atrisk[message][queue_to_serve][i]+1;
            }
        }
        /*END BLOCK*/

        /* Schedule a departure (service completion) for this server, and save the server number in attribute 3
        of the event list. */
        transfer[3]=depart_server;
        temp=floor(sim_time)+empric_cdf(lcgrand(STREAM),1); /*Randomly draw service time from empirical distribution of service times*/
        event_schedule(temp, EVENT_DEPARTURE);
        server_intime[depart_server]=sim_time;
    }
}

/*******************************************************************************************/

void abandon_decision(void)  /* Abandonment Decision function. */
{
    /*In this subroutine, we go through each caller in the online queue and determine whether they abandon in this period.*/

    /*Determine the queue length.*/
	queue_length_online=list_size[LIST_ONLINE_QUEUE];

	if (queue_length_online>0){ /*There are callers waiting in the online queue*/

	  for (aban_loop=1; aban_loop<=queue_length_online; ++aban_loop){ /*Loop through all the callers*/

		/*Remove the caller from the front of the online queue*/
		list_remove(FIRST,LIST_ONLINE_QUEUE);

        /*Update caller number and latent class*/
        caller_number = transfer[10];
        caller_class = Latent_Class[caller_number];

		/*Determine delay message subset from the transfer array*/
		online_message=transfer[4];

		/*Getting the delay which is the difference between the sim_time and time of arrival*/
		current_period=floor(sim_time-transfer[1])+1;

        /*Determine the nominal utilities of actions.*/
		v0 = 0; /*Nominal utility of abandoning*/
		v1 = r[caller_class][Evening] - c_n[caller_class][Evening]*EW[online_message][1][current_period]; /*nominal utility of waiting in online queue*/

        /*Determine probability of abandoning*/
        abandon_prob = exp(v0)/(exp(v0)+exp(v1));

        /*Generate random number for determining whether caller abandons*/
        temp = lcgrand(STREAM);

    	if (temp <= abandon_prob){ /*Caller Abandons*/

            /*Update Statistics*/
            if (num_custs_delayed>=transient-1){
                wait_time[1]=wait_time[1]+sim_time - transfer[1];
                ++calls_received[1];
                ++calls_abandoned;
            }

			/*Increment num_custs_delayed and record starttime if the transient threshold has been passed*/
			++num_custs_delayed;
            if (num_custs_delayed==transient){
                starttime=sim_time;
            }

            /*BEGIN BLOCK*/
            /*In this block, we add the waiting time of this answered call to a table for figuring out pt (the service probabilities at the beginning of the next iteration.*/
			delay=floor(sim_time-transfer[1]);
			if (delay>0 && num_custs_delayed>=transient){
				for (i=1; i<=delay; ++i){
				atrisk[online_message][1][i]=atrisk[online_message][1][i]+1;
				}
			}
			/*END BLOCK*/

            /*Schedule next arrival for caller*/
            next_arrival_period = ceil(expon(Avg_Interstring_Time[caller_class],STREAM)); /*Generate from caller's arrival rate*/
            event_schedule(sim_time+next_arrival_period,EVENT_ARRIVAL);

		}else{ /*Caller chooses to wait in online queue. Place back in queue.*/
			list_file(LAST,LIST_ONLINE_QUEUE);
		}
	  } /*Done looping through callers*/
	}
	/*Schedule the next abandonment decision event one period from now*/
	event_schedule(sim_time+1,EVENT_ABANDON_DECISION);
}

/*******************************************************************************************/

int  empric_cdf(float cdf_value, int arr_serv_no) /*To use the emprical distribution of inter arrival and service times.*/
{
	for (i_cdf=1; i_cdf<=(cdf_size[arr_serv_no]); ++i_cdf){
		if (cdf[arr_serv_no][i_cdf]>cdf_value && cdf[arr_serv_no][i_cdf-1]<=cdf_value){
			output_cdf=i_cdf-1;
		}
	}
	return output_cdf;
}

/*******************************************************************************************/

void record(void)  /* Report generator function. */
{
    /* Get and write out estimates of desired measures of performance. */

    for (i=1; i<=n_servers; ++i){
		server_busy_time[i]=server_busy_time[i]+server_status[i]*(sim_time-server_intime[i]);
		server_util[i]=server_busy_time[i]/sim_time;
	}

	server_util_sum=0;
	for (i=1; i<=n_servers; ++i){
		       server_util_sum = server_util_sum+server_util[i];
	   }
    server_util_total = server_util_sum/n_servers;

    wait_time_total=wait_time[1]+wait_time[2];
    calls_received_total=calls_received[1]+calls_received[2];
    calls_answered_total=calls_answered[1]+calls_answered[2];
    calls_not_serviced=calls_abandoned+callbacks_not_answered;

    AWT[1]=wait_time[1]/calls_received[1] * period_length;
    AWT[2]=wait_time[2]/calls_received[2] * period_length;
    AWT_total=wait_time_total/calls_received_total * period_length;

    abandon_rate=calls_abandoned/calls_received[1];
    callback_not_answer_rate=callbacks_not_answered/calls_received[2];
    no_service_rate=calls_not_serviced/calls_received_total;

    avg_queue_length[1]=filest(1);
    avg_queue_length[2]=filest(2);
    avg_queue_length_total= avg_queue_length[1]+avg_queue_length[2];

    percent_accept_callback= callbacks_accepted / callbacks_offered;
    percent_answer_callback= calls_answered[2] / callbacks_accepted;
    AWT_All= wait_time[1] / calls_received_total * period_length;
    Throughput = calls_answered_total / (sim_time-starttime);
    Rho_On = (calls_received[1]/(sim_time-starttime))/(n_servers/(avg_service_time*periods_per_minute)-calls_answered[2]/(sim_time-starttime));
    Rho_All = (calls_received_total/(sim_time-starttime))/(n_servers/(avg_service_time*periods_per_minute));

    fprintf(outfile,"%d,%d,%d,%16.10f,%16.10f,%16.10f,%16.10f,%16.10f,%16.10f,%16.10f,%16.10f,%16.10f,%16.10f,%16.10f,%16.10f,%16.10f,%16.10f,%16.10f,%16.10f,%16.10f,%16.10f,%16.10f,%16.10f,%16.10f,%16.10f,%16.10f,%16.10f,%16.10f,%16.10f,%16.10f\n"
        ,iter,n_servers,policy_number,Throughput,AWT_All,AWT[1],Rho_On,Rho_All,AWT[1],AWT[2],AWT_total,
        calls_received[1],calls_received[2],calls_received_total,
        calls_answered[1],calls_answered[2],calls_answered_total,
        calls_abandoned,callbacks_not_answered,calls_not_serviced,
        abandon_rate,callback_not_answer_rate,no_service_rate,
        avg_queue_length[1],avg_queue_length[2],avg_queue_length_total,
        server_util_total,sim_time-starttime,
        percent_accept_callback,percent_answer_callback);
}
