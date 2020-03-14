    #include <iostream>
    using std::cout;
    #include <fstream>
    using std::ifstream;
    #include <string>
    using std::string;
    using std::getline;
    size_t count_lines(const char *filename)
    {
       ifstream myfile("test.txt");
       string line;
       size_t count = 0;
       while ( getline(myfile, line) )
       {
          ++count;
       }
       return count;
    }
    int main()
    {
       const char filename[] = "test.txt";
       size_t i, count = count_lines(filename);
       ifstream myfile(filename);
       string line;
       for ( i = 0; i < count - 10; ++i )
       {
          getline(myfile, line); /* read and discard: skip line */
       }
       while ( getline(myfile, line) )
       {
          cout << line << "\n";
       }
       return 0;
    }
