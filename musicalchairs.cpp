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
#include <vector>
#include <random>


/*
 * Forward declarations
 */

void usage(int argc, char *argv[]);
unsigned long long musical_chairs();

using namespace std;

bool music_stopped;
mutex mtx_music_stopped;
mutex mtx_victim;
int victim;
mutex mtx_elimination;
condition_variable elimination;
vector<int> free_chairs;
vector<bool> is_chair_free;
vector<unsigned long long> sleep_duration;
vector<mutex> mtx_cv;
vector<condition_variable> cv;
mutex mtx_alive_players;
vector<int> alive_players;
int nplayers; 

vector<thread> player_threads;
enum class umpire_state
  {
    wait_lapstart,
    wait_playersleep_musicstart,
    wait_umpiresleep_musicstop,
    wait_victim,
    wait_lapstop,
    exit
  };

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
    umpire_state state = wait_lapstart;
    while (true)
      {
        if (state == umpire_state::exit)
          break;
        switch (state)
          {
          case umpire_state::wait_lapstart:
            state = waiting_lapstart ();
            break;
          
          case umpire_state::wait_playersleep_musicstart:
            state = waiting_playersleep_musicstart ();
            break;

          case umpire_state::wait_umpiresleep_musicstop:
            state = waiting_umpiresleep_musicstop ();
            break;

          case umpire_state::wait_victim:
            state = waitng_victim ();
            break;

          case umpire_state::wait_lapstop:
            state = waiting_lapstop ();
            break;
      }
	return;
}

umpire_state waiting_lapstart ()
  {
    string input;
    cin >> input;
    if (input != "lap_start")
      {
        // handle error
      }
    return umpire_state::wait_playersleep_musicstart; 
  }

umpire_state waiting_playersleep_musicstart ()
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
    return umpire_state::wait_umpiresleep_musicstop;
  }

umpire_state waiting_umpiresleep_musicstop ()
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
    return umpire_state::wait_victim;
  }

umpire_state waiting_victim ()
  {
    unique_lock<mutex> lck_elimination (mtx_elimination);
    elimination.wait (lck_elimination);

    int plid;
    unique_lock<mutex> lck_victim (mtx_victim);
    plid = victim;
    lck_victim.unlock ();
    player_threads[plid].join ();
    return umpire_state::wait_lapstop;
  }

umpire_state waiting_lapstop ()
  {
    string input;
    cin >> input;
    if (input != "lap_stop")
      {
        // handle error
      }

    unique_lock<mutex> lck_music_stopped (mtx_music_stopped);
    music_stopped = false;
    lck_music_stopped.unlock ();

    unique_lock<mutex> lck_alive_players (mtx_alive_players);
    int lap_no = nplayers - alive_players.size ();
    lck_alive_players.unlock ();

    cout << "======= lap# " << lap_no << " =======\n";

    unique_lock<mutex> lck_victim (mtx_victim);
    int plid = victim;
    lck_victim.unlock ();

    cout << plid << " could not get chair\n**********************";

    lck_alive_players.lock ();
    if (alive_players.size () == 1)
      {
        plid = alive_players[0];
        cout << "Winner is " << plid << "\n";
        player_threads[plid].join ();
        return umpire_state::exit;
      }
    else
      {
        return umpire_state::wait_lapstart;
      }
  }

void player_main(int plid)
{
    /* Add your code here */
	/* synchronize stdouts coming from multiple players */
	return;
}

int pick_a_chair ()
  {
    static random_device dev;
    static mt19937 rng (dev);

    uniform_int_distribution<mt19937::result_type> dist(0, free_chairs.size()-1);
    return free_chairs[dist(rng)];
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

