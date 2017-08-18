#include <fstream>    // For ifstream, ofstream
#include <iomanip>    // For setprecision()
#include <iostream>   // For cin, cout, fixed, and endl
#include <string>
#include <map>

using namespace std;

//  289585,444902 write /media/sf_fio/debug/scheduler

int main(int argc, char *argv[]){

  ifstream fin;
  ofstream fout;

  string str;
  string file_contents;
  string file_name, search_latency, search_io_write, search_io_read, lat_usec_check, lat_msec_check;
  int converted_numbers, p1, p2;
  double converted_double;
  int usec_multiply;
  int latency_total;
  double throughput_total;
  int total_threads;


  if(argc == 1){
      cout << "no arguments (exit)" << endl;
      return 0;
  }

  file_name = argv[1];
  cout << "parsing filename " << file_name << endl;

  search_latency = "| 99.99th=[";
  search_io_write = "READ: bw=";
  search_io_read = "WRITE: bw=";
  lat_usec_check = "clat percentiles (usec):";
  lat_msec_check = "clat percentiles (msec):";
  usec_multiply= 1;
  total_threads = latency_total = throughput_total = 0;



  fin.open(file_name);
  fout.open(file_name+"_output");

//      | 99.99th=[ 4640]

//    READ: io=958152KB, aggrb=47898KB/s, minb=47898KB/s, maxb=47898KB/s, mint=20004msec, maxt=20004msec

file_contents += "-------latency_results-------";
file_contents.push_back('\n');

  while (getline(fin, str)){

    if (str.find(lat_usec_check) != string::npos) {
        usec_multiply=1;
    }

    if (str.find(lat_msec_check) != string::npos) {
        usec_multiply = 1000;
    }


    if (str.find(search_latency) != string::npos) {
      p1 = str.find("[");
      p2 = str.find("]");


      str =  str.substr(p1 + 1, p2 - p1 - 1);

          converted_numbers = stoi( str ) * usec_multiply;
          str = to_string(converted_numbers);
          file_contents += str;
          file_contents.push_back('\n');

          latency_total += converted_numbers;

          total_threads++;
    }


    if ((str.find(search_io_write) != string::npos) || (str.find(search_io_read) != string::npos)) {

      file_contents += "-------io_throughput-------";
      file_contents.push_back('\n');

      p1 = str.find("io=");
      p2 = str.find("run=");

      str =  str.substr(p1 + 3, p2 - p1 - 4);
      // converted_numbers = stoi( str );

      p1 = str.find("(");
      p2 = str.find(")");
      str =  str.substr(p1 + 1, p2 - p1 - 1);


      if(str.find("MB") != string::npos){
          p2 = str.find("MB");
          str =  str.substr(0, p2);

          converted_double = stod( str )*1000;

          str = to_string(converted_numbers);
      }else{
          p2 = str.find("KB");
          str =  str.substr(0, p2);
          converted_double = stod( str );
      }

          throughput_total += converted_double;
          file_contents += str;
          file_contents.push_back('\n');
    }


  }
  fin.close();
  fout << file_contents;
  fout.close();

  cout << "total_jobs: " << total_threads << endl
       << "latency_mean: " << latency_total/total_threads << endl
       << "throughput_mean: " << throughput_total/total_threads << endl;

  return 0;
}
