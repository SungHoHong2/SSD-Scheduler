#include <fstream>    // For ifstream, ofstream
#include <iomanip>    // For setprecision()
#include <iostream>   // For cin, cout, fixed, and endl
#include <string>
using namespace std;


int main(int argc, char *argv[]){
    ifstream fin;
    ofstream fout;

    string str;
    string file_contents;
    string file_name, search_keyword;
    int usec, p1, p2;
    int converted_numbers;
    int total_latency = 0;

    if(argc == 1){
        cout << "no arguments (exit)" << endl;
        return 0;
    }

    file_name = argv[1];
    cout << "parsing filename " << file_name << endl;

      // search_keyword = "IOPS=";
      // search_keyword = "clat (usec): min="; search_keyword_2 = "clat (msec): min=";
       search_keyword = "99.99th=[";

    cout << file_name << endl;
    fin.open(file_name);
    // fout.open("result_latency_"+file_name);

    while (getline(fin, str)){
      if (str.find("clat percentiles (usec):") != string::npos) {
            usec = 1;
      }
      if (str.find("clat percentiles (msec):") != string::npos) {
            usec = 1000;
      }

      if (str.find(search_keyword) != string::npos) {

          p1 = str.find("99.99th=[");
          p2 = str.find("]");

          str =  str.substr(p1 + 9, p2 - p1 - 1);

          total_latency += converted_numbers = stod( str ) * usec;


          str = to_string(converted_numbers);
          file_contents += str;
          file_contents.push_back('\n');

      }



    }
    fin.close();

    cout << "result: " << total_latency << endl;

    // fout << file_contents;
    // fout.close();

    return 0;
}
