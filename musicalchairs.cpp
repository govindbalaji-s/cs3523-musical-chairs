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
#include <algorithm>
#include <vector>
#include <random>
#include <thread>
#include <mutex>
#include <condition_variable>


/*
 * Forward declarations
 */

void usage(int argc, char *argv[]);
unsigned long long musical_chairs();
void waiting_lapstart ();
void waiting_playersleep_musicstart ();
void waiting_umpiresleep_musicstop ();
void waiting_victim ();
int waiting_lapstop ();

int idle_player (int plid);
int going_around (int plid);
int hunting_chairs (int plid);
int pick_a_chair ();


using namespace std;

bool music_stopped = false;
mutex mtx_music_stopped;
mutex mtx_sleep_duration;
mutex mtx_victim;
int victim;
mutex mtx_elimination;
condition_variable elimination;
mutex mtx_chair;
vector<int> free_chairs;
vector<bool> is_chair_free;
vector<unsigned long long> sleep_duration;
vector<mutex> mtx_cv;
vector<condition_variable> cv;
mutex mtx_alive_players;
vector<int> alive_players;

mutex mtx_unready_count;
int unready_count;
mutex mtx_all_ready;
condition_variable cv_all_ready;
int nplayers; 

bool is_lap_starting;
mutex mtx_lap_starting;
condition_variable cv_lap_starting;

vector<thread> player_threads;

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
	game_time = musical_chairs();

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
 // cout << "ump" << endl;
    while (true)
      {
         // cout << "came to next lap" << endl;
        waiting_lapstart ();
            
		    waiting_playersleep_musicstart ();
    	
    	  waiting_umpiresleep_musicstop ();
 		
 		    waiting_victim ();
        
        if (waiting_lapstop () == 1)
          break;
          // cout << "going to next lap" << endl;
      }
	return;
}

void waiting_lapstart ()
  {
    string input;
    cin >> input;
    /*if (input != "lap_start")
      {
        // handle error
      }*/


    unique_lock<mutex> lck_lap_starting (mtx_lap_starting);
    is_lap_starting = true;
    cv_lap_starting.notify_all ();
    lck_lap_starting.unlock ();
        // cout << "umpire waitin fo rplayers" << endl;

    while (unready_count != 0)
      {
        unique_lock<mutex> lck_all_ready (mtx_all_ready);
        cv_all_ready.wait (lck_all_ready);
      }
    // cout << "umpire done waiting" << endl;

    is_lap_starting = false;
  }

void waiting_playersleep_musicstart ()
  {
    string input;
    while (true)
      {
        cin >> input;
        if (input == "music_start")
          break;
        int plid;
        cin >> plid;

        unique_lock<mutex> lck_cv (mtx_cv[plid]);
        unique_lock<mutex> lck_sleep (mtx_sleep_duration);
          cin >> sleep_duration[plid];
        lck_sleep.unlock ();

        cv[plid].notify_one ();
        lck_cv.unlock ();
      }
    
  }

void waiting_umpiresleep_musicstop ()
  {
    string input;
    while (true)
      {
        cin >> input;
        if (input == "music_stop")
          break;
        unsigned long long umpire_sleep_time;
        cin >> umpire_sleep_time;
        this_thread::sleep_for (chrono::microseconds (umpire_sleep_time));
      }
    unique_lock<mutex> lck_music_stopped (mtx_music_stopped);
    music_stopped = true;
    lck_music_stopped.unlock ();

    for (int plid = 0; plid < nplayers; ++plid)
      {
        unique_lock<mutex> lck_cv (mtx_cv[plid]);
        unique_lock<mutex> lck_sleep (mtx_sleep_duration);
        sleep_duration[plid] = 0;
        cv[plid].notify_one ();
      }

  }

void waiting_victim ()
  {

    // cout << "umpire waiting for victim" << endl;
    int plid;
    unique_lock<mutex> lck_victim (mtx_victim);
    plid = victim;
   // victim = -1;
    lck_victim.unlock ();
    while (plid == -1)
      {
        unique_lock<mutex> lck_elimination (mtx_elimination);
        elimination.wait (lck_elimination);
        lck_victim.lock();
        plid = victim;
        lck_victim.unlock ();
      }
    // cout << "umpire signalled victim" << endl;

    player_threads[plid].join ();
  
  }

int waiting_lapstop ()
  {
    string input;
    cin >> input;
    /*if (input != "lap_stop")
      {
        // handle error
      }*/

    unique_lock<mutex> lck_music_stopped (mtx_music_stopped);
    music_stopped = false;
    lck_music_stopped.unlock ();

    unique_lock<mutex> lck_alive_players (mtx_alive_players);
    int lap_no = nplayers - alive_players.size ();
    unique_lock<mutex> lck_chair (mtx_chair);
    for(int i = 0; i < alive_players.size()-1; i++)
      {
        free_chairs.push_back (i);
        is_chair_free[i] = true;
      }
    lck_chair.unlock ();

    unique_lock<mutex> lck_unready_count (mtx_unready_count);
    unready_count += alive_players.size ();
    lck_unready_count.unlock ();
    lck_alive_players.unlock ();

    cout << "======= lap# " << lap_no << " =======" << endl;

    unique_lock<mutex> lck_victim (mtx_victim);
    int plid = victim;
    victim = -1;
    lck_victim.unlock ();

    cout << plid << " could not get chair\n**********************" << endl;

    lck_alive_players.lock ();
    if (alive_players.size () == 1)
      {
        plid = alive_players[0];
        cout << "Winner is " << plid << endl;
        lck_alive_players.unlock ();
        // cout << "umpire waiting for cv lock" << endl;
        unique_lock<mutex> lck_lap_starting (mtx_lap_starting);
        cv_lap_starting.notify_all ();
        // cout << "umpire waiting for winner to end celebration" << endl;
        lck_lap_starting.unlock ();
        player_threads[plid].join ();
        return 1;
      }
    else
      {
        //cout << "returning 0 only" << endl;
        return 0;
      }
  }

void player_main(int plid)
{
  /* Add your code here */
	/* synchronize stdouts coming from multiple players */
	// cout << "pl" << plid << endl;
	while(true)
	  {
      // cout << "id" << plid << endl;
      if (idle_player (plid) == 1)
        break;
       // cout << "ga" << plid << endl;
	  	if (going_around(plid) == 1)
        break;
        // cout << "gae" << plid << endl;
	  	if(hunting_chairs (plid) == -1)
	  		break;
       // cout << "hce" << plid << endl;
	  }
   // cout << "pme" << plid << endl;
	
	return;
}

int idle_player (int plid)
  {

    

    // cout << "waiting for lap to start" << plid << endl;
    while (!is_lap_starting)
      { 
        unique_lock<mutex> lck_lap_starting (mtx_lap_starting);
        unique_lock<mutex> lck_alive_players (mtx_alive_players);
        //check if game won
        // cout << "getting alvie lock" << plid << endl;

        if (alive_players.size () == 1)
          return 1;
        lck_alive_players.unlock ();
        cv_lap_starting.wait (lck_lap_starting);
      }

    // cout << "player is ready" << plid << endl;
    unique_lock<mutex> lck_unready_count (mtx_unready_count);
    unready_count --;
    // cout << "still unready:" << unready_count << endl;
    unique_lock<mutex> lck_all_ready (mtx_all_ready);
    cv_all_ready.notify_one ();
    //cout << "umpire told we are ready" << endl;
    lck_unready_count.unlock();
  }
int going_around(int plid)        //waits for sleep or music_stop
{
	bool mstop; //bool ready_signalled = false;
  // cout << plid << "is going around" << endl;
  unique_lock<mutex>lck_music_stopped(mtx_music_stopped);
  mstop = music_stopped;
  lck_music_stopped.unlock();
	while (!mstop)
	  {
      // cout << "waiting for lck_cv#" << plid << endl;
      // cout << "waiting for cv lock " << plid << endl;
      unique_lock<mutex> lck_cv (mtx_cv[plid]);
      // if (!ready_signalled)
      //   {
      //     ready_signalled = true;
      //   }
	    cv[plid].wait (lck_cv);

      // cout <<"waiting for alive lock" << plid << endl;
      

      //cout << "in ga: lck_cv obtained or whatever#" << plid << endl;
	    unique_lock<mutex>lck_sleep_duration(mtx_sleep_duration);
	    unsigned long long slp = sleep_duration[plid];
	    lck_sleep_duration.unlock();

	    if (slp > 0)
	      {
            this_thread::sleep_for(chrono::microseconds(slp));  //umpire thread changes the value of sleep[plid] to 0
	      }	

	    unique_lock<mutex>lck_music_stopped(mtx_music_stopped);
	    mstop = music_stopped;
	    lck_music_stopped.unlock();

	  }
    return 0;
}

int hunting_chairs(int plid)
{
  // cout << plid << "is hunting." << endl;
	/*function for finding chairs*/
	while(true)
	  {
	    unique_lock<mutex>lck_chair (mtx_chair);
      // cout << "free chairs:";
      // for(int c : free_chairs)
      //   cout << c << '#';
      // cout << endl;
	    if(free_chairs.size () == 0)
	      {	 
           // cout << "i am dying" << plid << endl;
	      	unique_lock<mutex>lck_alive_players (mtx_alive_players);     	
	      	alive_players.erase(remove(alive_players.begin(), alive_players.end(), plid), alive_players.end());    //remove from alive players
	      	lck_alive_players.unlock();
          // cout << "me removed" << plid << endl;
	      	unique_lock<mutex>lck_victim(mtx_victim);
	      	victim = plid;
	      	lck_victim.unlock();

	      	unique_lock<mutex>lck_elimination(mtx_elimination);
	      	elimination.notify_one();
	      	lck_elimination.unlock();
          // cout << "death signalled" << plid << endl;
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

int pick_a_chair ()
  {
    static mt19937 rng(chrono::steady_clock::now().time_since_epoch().count());

    uniform_int_distribution<mt19937::result_type> dist(0, free_chairs.size()-1);
    return free_chairs[dist(rng)];
  }

unsigned long long musical_chairs()
{
	auto t1 = chrono::steady_clock::now();

  cout << "Musical Chairs: " << nplayers << " player game with " << 
          nplayers - 1 << " laps." << endl;

  unique_lock<mutex> lck_chair (mtx_chair);
  for (int i = 0; i < nplayers - 1; i++)
    {
      free_chairs.push_back (i);
      is_chair_free.push_back (true);
    }
  lck_chair.unlock ();

  unique_lock<mutex> lck_alive_players (mtx_alive_players);
  unique_lock<mutex> lck_unready_count (mtx_unready_count);
  unready_count = nplayers;
  is_lap_starting = false;
  victim = -1;
  lck_unready_count.unlock ();
  for (int i = 0; i < nplayers; i++)
    {
      alive_players.push_back (i);
      sleep_duration.push_back (0);
    }
  lck_alive_players.unlock ();
  mtx_cv = vector<mutex>(nplayers);
  cv = vector<condition_variable>(nplayers);
	// Spawn umpire thread.
  /* Add your code here */

	// Spawn n player threads.
    /* Add your code here */
    thread umpire_thread (umpire_main);

  player_threads.resize (nplayers);
  for (int plid = 0; plid < nplayers; plid++)
    player_threads[plid] = thread (player_main, plid);

  // cout << "all players born" << endl;
  // cout << "umpire created" << endl;
  umpire_thread.join ();

	auto t2 = chrono::steady_clock::now();

	auto d1 = chrono::duration_cast<chrono::microseconds>(t2 - t1);

	return d1.count();
}

