/*
 * Program: Musical chairs game with n players and m intervals.
 * Author:  changeme  changeme
 * Roll# :  changeme
 */

#include <cstdlib>  /* for exit, atoi */
#include <iostream>  /* for fprintf */
#include <errno.h>   /* for error code eg. E2BIG */
#include <getopt.h>  /* for getopt */
#include <assert.h>  /* for assert */
#include <chrono>	/* for timers */


/*
 * Forward declarations
 */

void usage(int argc, char *argv[]);
unsigned long long musical_chairs();

using namespace std;

bool music_stopped = false;
mutex mtx_music_stopped;
mutex mtx_sleep_duration;
int victim;
mutex mtx_elimination;
condition_variable elimination;
mutex mtx_chair;
vector<int> free_chairs;
vector<bool> is_chair_free;
vector<condition_variable> cv;
int nplayers; 
int alive_players;

int main(int argc, char *argv[])
{
    int c;
	nplayers = 0;

    /* Loop through each option (and its's arguments) and populate variables */
    while (1) {
        int this_option_optind = optind ? optind : 1;
        int option_index = 0;
        static struct option long_options[] = {
            {"help",            no_argument,        0, 'h'},
            {"nplayers",         required_argument,    0, '1'},
            {0,        0,            0,  0 }
        };

        c = getopt_long(argc, argv, "h1:", long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 0:
            cerr << "option " << long_options[option_index].name;
            if (optarg)
                cerr << " with arg " << optarg << endl;
            break;

        case '1':
            nplayers = atoi(optarg);
            break;

        case 'h':
        case '?':
            usage(argc, argv);

        default:
            cerr << "?? getopt returned character code 0%o ??n" << c << endl;
            usage(argc, argv);
        }
    }

    if (optind != argc) {
        cerr << "Unexpected arguments.\n";
        usage(argc, argv);
    }


	if (nplayers == 0) {
		cerr << "Invalid nplayers argument." << endl;
		return EXIT_FAILURE;
	}

    unsigned long long game_time;
	game_time = musical_chairs(nplayers);

    cout << "Time taken for the game: " << game_time << " us" << endl;

    exit(EXIT_SUCCESS);
}

/*
 * Show usage of the program
 */
void usage(int argc, char *argv[])
{
    cerr << "Usage:\n";
    cerr << argv[0] << "--nplayers <n>" << endl;
    exit(EXIT_FAILURE);
}

void umpire_main()
{
    /* Add your code here */
	/* read stdin only from umpire */
	return;
}

void player_main(int plid)
{
    /* Add your code here */
	/* synchronize stdouts coming from multiple players */
	
	while(true)
	  {
	  	going_around();

	  	if(hunting_chairs () == -1)
	  		break;
	  }

	
	return;
}

void going_around()        //waits for sleep or music_stop

{
	bool mstop;

	do
	  {
	    cv[i].wait ();

	    unique_lock<mutex>lck_sleep_duration(mtx_sleep_duration);
	    unsigned long long slp = sleep_duration[i];
	    lck_sleep_duration.unlock();

	    if (slp > 0)
	      {
            this_thread::sleep_for(chrono::microseconds(slp));  //umpire thread changes the value of sleep[i] to 0
	      }	

	    unique_lock<mutex>lck_music_stopped(mtx_music_stopped);
	    mstop = music_stopped;
	    lck_music_stopped.unlock();

	  }while(!mstop)
}

int hunting_chairs(int plid)
{
	/*function for finding chairs*/

	int fchair;

	while(true)
	  {
	    unique_lock<mutex>lck_chair (mtx_chair);

	    if(free_chairs.size () == 0)
	      {	 
	      	unique_lock<mutex>lck_alive_players (mtx_alive_players);     	
	      	alive_players.erase(remove(alive_players.begin(), alive_players.end(), plid), alive_players.end());    //remove from alive players
	      	lck_alive_players.unlock();

	      	unique_lock<mutex>lck_victim(mtx_victim);
	      	victim = plid;
	      	lck_victim.unlock();

	      	unique_lock<mutex>lck_elimination(mtx_elimination)
	      	elimination.notify_one();
	      	lck_elimination.unlock();

	      	return -1;
	      }

		int i = pick_a_chair ();
		lck_chair.unlock ();

		lck_chair.lock ();

		if (is_chair_free[i])
		  {
		  	free_chairs.erase(remove(free_chairs.begin(), free_chairs.end(), i), free_chairs.end());     //remove it from free chairs list

		    is_chair_free[i] = false;
		    
		    return i;
		  }
		
		lck_chair.unlock ();

	  } 


}


unsigned long long musical_chairs()
{
	auto t1 = chrono::steady_clock::now();

	// Spawn umpire thread.
    /* Add your code here */

	// Spawn n player threads.
    /* Add your code here */

	auto t2 = chrono::steady_clock::now();

	auto d1 = chrono::duration_cast<chrono::microseconds>(t2 - t1);

	return d1.count();
}

