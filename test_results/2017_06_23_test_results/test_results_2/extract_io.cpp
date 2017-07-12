#include <fstream>    // For ifstream, ofstream
#include <iomanip>    // For setprecision()
#include <iostream>   // For cin, cout, fixed, and endl
#include <string>
using namespace std;


int main(){
    ifstream fin;
    ofstream fout;

    string str;
    string file_contents;
    string file_name, search_keyword;

    file_name = "sfq_test_4";
    // search_keyword = "io=";
    // search_keyword = "clat (usec): min=";
     search_keyword = "99.99th=[";

    cout << file_name << endl;
    fin.open(file_name+".txt");
    fout.open(file_name+"_extracted.txt");

    while (getline(fin, str)){
      if (str.find(search_keyword) != string::npos) {
        file_contents += str;
        file_contents.push_back('\n');
      }
    }
    fin.close();


    fout << file_contents;
    fout.close();

    return 0;
}
