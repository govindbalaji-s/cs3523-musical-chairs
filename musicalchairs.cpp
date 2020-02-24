/*
 * Program: Musical chairs game with n players and m intervals.
 * Author:  Govind Balaji S,  K Havya Sree
 * Roll# :  cs18btech11015, cs18btech11022
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
#include <shared_mutex>

using namespace std;

/*
 * Forward declarations
 */

void                usage                          (int argc,
                                                    char *argv[]);
unsigned long long  musical_chairs                 ();

void                waiting_lapstart               ();
void                waiting_playersleep_musicstart ();
void                waiting_umpiresleep_musicstop  ();
void                waiting_victim                 ();
int                 waiting_lapstop                ();

int                 idle_player                    (int plid);
void                going_around                   (int plid);
void                looking_to_sleep               (int plid);
int                 hunting_chairs                 (int plid);                                                  
int                 pick_a_chair                   ();

bool                        music_stopped;
shared_timed_mutex          mtx_music_stopped;
condition_variable_any      cv_music_stopped;

int                         victim;
mutex                       mtx_elimination;
condition_variable          elimination;
//TODO winner sync
vector<int>                 free_chairs;
vector<bool>                is_chair_free;
shared_timed_mutex          pick_throw_mtx;
vector<mutex>               single_chair;

vector<unsigned long long>  sleep_duration;
vector<mutex>               mtx_sleep_duration;

mutex                       mtx_alive_players;
vector<int>                 alive_players;

int                         unready_count;
mutex                       mtx_all_ready;
condition_variable          cv_all_ready;

bool                        winner;
bool                        is_lap_starting;
shared_timed_mutex          mtx_lap_starting;
condition_variable_any      cv_lap_starting;

bool                        is_music_start;
shared_timed_mutex           mtx_music_started;
condition_variable_any      cv_music_started;

int                         nplayers;
vector<thread>              player_threads;

#define DEBUG false

int
main (int argc,
      char *argv[])
  {
    int c;
  	nplayers = 0;

    /* Loop through each option (and its's arguments) and populate variables */
    while (1)
      {
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
void
usage (int argc,
       char *argv[])
  {
    cerr << "Usage:\n";
    cerr << argv[0] << "--nplayers <n>" << endl;
    exit(EXIT_FAILURE);
  }

void
umpire_main (void)
  {
    while (true)
      {
        waiting_lapstart ();
            
		    waiting_playersleep_musicstart ();
    	
    	  waiting_umpiresleep_musicstop ();
 		
 		    waiting_victim ();
        
        if (waiting_lapstop () == 1)
          break;
      }
  }

void
reset_globals (void)
  {
    unique_lock<shared_timed_mutex> lck_lap_starting (mtx_lap_starting);
    is_lap_starting = false;
    winner          = false;
    lck_lap_starting.unlock ();

    for (int plid = 0; plid < nplayers; ++plid)
      {
        unique_lock<mutex> lck_sleep_duration (mtx_sleep_duration[plid]);
        sleep_duration[plid] = 0;
        lck_sleep_duration.unlock ();
      }

    unique_lock<shared_timed_mutex> lck_music_started (mtx_music_started);
    is_music_start  = false;
    lck_music_started.unlock ();

    unique_lock<shared_timed_mutex> lck_music_stopped (mtx_music_stopped);
    music_stopped   = false;
    lck_music_stopped.unlock ();

    unique_lock<mutex> lck_elimination (mtx_elimination);
    victim          = -1;
    lck_elimination.unlock ();

    unique_lock<mutex> lck_alive_players (mtx_alive_players);
    unique_lock<shared_timed_mutex> throw_lck (pick_throw_mtx);
    for(int i = 0; i < alive_players.size () - 1; i++)
      {
        free_chairs.push_back (i);
        unique_lock<mutex> lck_sit (single_chair[i]);
        is_chair_free[i] = true;
        lck_sit.unlock ();
      }
    throw_lck.unlock ();

    unique_lock<mutex> lck_all_ready (mtx_all_ready);
    unready_count = alive_players.size ();
    lck_all_ready.unlock ();

    cout << "======= lap# " << nplayers - alive_players.size () + 1<< " =======" << endl;

    lck_alive_players.unlock ();
  }

void
waiting_lapstart (void)
  {

    reset_globals ();

    string input;
    cin >> input;

    unique_lock<shared_timed_mutex> lck_lap_starting (mtx_lap_starting);
    is_lap_starting = true;
    lck_is_lap_starting.unlock ();

    unique_lock<mutex> lck_lap_starting (mtx_lap_starting);
    cv_lap_starting.notify_all ();
    lck_lap_starting.unlock ();

    if(DEBUG) cout << "umpire waitin fo rplayers" << endl;

    unique_lock<mutex> lck_all_ready (mtx_all_ready);
    while (unready_count != 0)
      cv_all_ready.wait (lck_all_ready);
    lck_all_ready.unlock ();

    if(DEBUG) cout << "umpire done waiting" << endl;

    lck_lap_starting.lock ();
    is_lap_starting = false;
    lck_lap_starting.unlock ();
  }

void
waiting_playersleep_musicstart (void)
  {
    string input;
    while (true)
      {
        cin >> input;
        if (input == "music_start")
          {
            unique_lock<shared_timed_mutex> lck_music_started (mtx_music_started);
            is_music_start = true;
            cv_music_started.notify_all ();
            lck_music_started.unlock ();
            break;
          }

        int plid;
        cin >> plid;

        unique_lock<mutex> lck_sleep (mtx_sleep_duration[plid]);
        cin >> sleep_duration[plid];
        lck_sleep.unlock ();
      }    
  }

void
waiting_umpiresleep_musicstop (void)
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

    unique_lock<shared_timed_mutex> lck_music_stopped (mtx_music_stopped);
    music_stopped = true;
    cv_music_stopped.notify_all ();
    lck_music_stopped.unlock ();
  }

void
waiting_victim (void)
  {
    if(DEBUG) cout << "umpire waiting for victim" << endl;

    unique_lock<mutex> lck_elimination (mtx_elimination);
    while (victim == -1)
      elimination.wait (lck_elimination);

    cout << victim << " could not get chair\n**********************" << endl;
    //player_threads[victim].join ();
    lck_elimination.unlock ();

    if(DEBUG) cout << "umpire signalled victim" << endl;  
  }

int
waiting_lapstop (void)
  {
    string input;
    cin >> input;

    unique_lock<mutex> lck_alive_players (mtx_alive_players);
    if (alive_players.size () == 1)
      {
        cout << "Winner is " << alive_players[0] << endl;
        lck_alive_players.unlock ();

        // if(DEBUG)  cout << "umpire waiting for cv lock" << endl;
        unique_lock<shared_timed_mutex> lck_lap_starting (mtx_lap_starting);
        is_lap_starting = winner = true;
        cv_lap_starting.notify_all ();
        if(DEBUG)  cout << "umpire waiting for winner to end celebration" << endl;
        lck_lap_starting.unlock ();
        //player_threads[alive_players[0]].join ();

        return 1;
      }
    return 0;
  }

void
player_main (int plid)
  {
    while(true)
  	  {
        if (idle_player (plid) == 1)
          break;

        looking_to_sleep (plid);

  	  	going_around (plid);

  	  	if(hunting_chairs (plid) == -1)
  	  		break;
  	  }
  }

int
idle_player (int plid)
  {
    if(DEBUG)  cout << "waiting for lap to start" << plid << endl;
    
    shared_lock<shared_timed_mutex> lck_lap_starting (mtx_lap_starting);
    while (!is_lap_starting)
      cv_lap_starting.wait (lck_lap_starting);
    if (winner)
      return 1;
    lck_lap_starting.unlock ();

    if(DEBUG)  cout << "player is ready" << plid << endl;
    
    unique_lock<mutex> lck_all_ready (mtx_all_ready);
    unready_count --;
    //if (unready_count == 0)
      cv_all_ready.notify_one ();
    lck_all_ready.unlock ();

    if(DEBUG) cout << "umpire told we are ready" << endl;
  }

void
looking_to_sleep (int plid)
  {
    shared_lock<shared_timed_mutex> lck_music_started (mtx_music_started);
    while (!is_music_start)
      cv_music_started.wait (lck_music_started);
    lck_music_started.unlock ();

    unique_lock<mutex> lck_sleep_duration (mtx_sleep_duration[plid]);
    if (sleep_duration[plid] > 0)
      this_thread::sleep_for(chrono::microseconds(sleep_duration[plid]));
    lck_sleep_duration.unlock ();
  }

void
going_around (int plid)        //waits for sleep or music_stop
{
  if(DEBUG)  cout << plid << "is going around" << endl;

  shared_lock<shared_timed_mutex> lck_music_stopped (mtx_music_stopped);
  while (!music_stopped)
    cv_music_stopped.wait (lck_music_stopped);
  lck_music_stopped.unlock ();
}

int
hunting_chairs (int plid)
{
  if(DEBUG)  cout << plid << "is hunting." << endl;
	while(true)
	  {
      shared_lock<shared_timed_mutex> pick_lck (pick_throw_mtx);
	    if(free_chairs.size () == 0)
	      {	 
          pick_lck.unlock ();
           if(DEBUG)  cout << "i am dying" << plid << endl;

	      	unique_lock<mutex> lck_alive_players (mtx_alive_players);     	
	      	alive_players.erase (remove (alive_players.begin (),
                                       alive_players.end (),
                                       plid),
                               alive_players.end ());    //remove from alive players
	      	lck_alive_players.unlock ();

          if(DEBUG)  cout << "me removed" << plid << endl;

	      	unique_lock<mutex> lck_elimination (mtx_elimination);
          victim = plid;
	      	elimination.notify_one ();
	      	lck_elimination.unlock ();

          if(DEBUG)  cout << "death signalled" << plid << endl;
	      	return -1;
	      }

		int i = pick_a_chair ();
    pick_lck.unlock ();

    unique_lock<mutex> lck_sit (single_chair[i]);
		if (is_chair_free[i])
		  {
        is_chair_free[i] = false;
        lck_sit.unlock ();

        unique_lock<shared_timed_mutex> throw_lck (pick_throw_mtx);
		  	free_chairs.erase (remove (free_chairs.begin (),
                                   free_chairs.end (),
                                   i),
                           free_chairs.end ());     //remove it from free chairs list
        throw_lck.unlock ();
		    
		    return i;
		  }
		lck_sit.unlock ();
	  }
}

int
pick_a_chair (void)
  {
    static mt19937 rng (chrono::steady_clock::now ().time_since_epoch ().count ());
    uniform_int_distribution<mt19937::result_type> dist (0, free_chairs.size ()-1);
    return free_chairs[dist (rng)];
  }

unsigned long long
musical_chairs(void)
{
	auto t1 = chrono::steady_clock::now ();

  cout << "Musical Chairs: " << nplayers << " player game with " << 
          nplayers - 1 << " laps." << endl;

  single_chair = vector<mutex> (nplayers - 1);
  is_chair_free = vector<bool> (nplayers - 1);

  mtx_sleep_duration = vector<mutex> (nplayers);
  sleep_duration = vector<unsigned long long> (nplayers);


  unique_lock<mutex> lck_alive_players (mtx_alive_players);
  for (int i = 0; i < nplayers; i++)
    alive_players.push_back (i);
  lck_alive_players.unlock ();

  thread umpire_thread (umpire_main);

  player_threads.resize (nplayers);
  for (int plid = 0; plid < nplayers; plid++)
    player_threads[plid] = thread (player_main, plid);

  umpire_thread.join ();
  for (int plid = 0; plid < nplayers; plid ++)
    player_threads[plid].join ();

	auto t2 = chrono::steady_clock::now();

	auto d1 = chrono::duration_cast<chrono::microseconds>(t2 - t1);

	return d1.count();
}
