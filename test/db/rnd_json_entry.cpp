// https://github.com/tincandevelop/random-sentence-generator
#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <stdlib.h>
#include <random>
#include <chrono>
#include <cassert>

int randBetween( int min, int max ) {
  return (rand() % (min - max) + min);
}

double randBetween( double min, double max ) {
  assert( min < max );
  return min + static_cast< double >( rand() ) / static_cast< double >( RAND_MAX/(max-min) );
}

using namespace std;

int main( int argc, char** argv ) {

  if (argc != 2) {
     cout << "Usage: " << argv[0] << " <path-to-txt-files>\n";
     return EXIT_FAILURE;
  }

  string prefix( argv[1] );
  prefix += '/';

  using namespace chrono;
  srand( duration_cast< microseconds >( steady_clock::now().time_since_epoch() ).count() );
  vector<string> pronoun;
  vector<string> adverb;
  vector<string> verb;
  vector<string> preposition;
  vector<string> adjective;
  vector<string> noun;

  string line;

  ifstream _pronoun(prefix+"pronoun.txt");
  while(getline(_pronoun,line)){
    pronoun.push_back(line);
  }
  _pronoun.close();

  ifstream _adverb(prefix+"adverb.txt");
  while(getline(_adverb,line)){
    adverb.push_back(line);
  }
  _adverb.close();

  ifstream _verb(prefix+"verb.txt");
  while(getline(_verb,line)){
    verb.push_back(line);
  }
  _verb.close();

  ifstream _preposition(prefix+"preposition.txt");
  while(getline(_preposition,line)){
    preposition.push_back(line);
  }
  _preposition.close();

  ifstream _adjective(prefix+"adjective.txt");
  while(getline(_adjective,line)){
    adjective.push_back(line);
  }
  _adjective.close();

  ifstream _noun(prefix+"noun.txt");
  while(getline(_noun,line)){
    noun.push_back(line);
  }
  _noun.close();

  auto lastname = adverb[randBetween(0,adverb.size()-1)];
  lastname[0] = toupper( lastname[0] );

  auto thing = [&](){ return noun[randBetween(0,noun.size()-1)]; };
  auto adj = [&](){ return adjective[randBetween(0,adjective.size()-1)]; };

  auto keyword = [&]( int num ){
    std::string k;
    for (int i=0; i<num; ++i) k += thing() + ';';
    k.pop_back();
    return k;
  };

  auto product = adj();
  product[0] = toupper( product[0] );
  product += ' ' + thing();

  auto sentence = [&]( int num ){
    std::string d;
    for (int i=0; i<num; ++i)
      d += pronoun[randBetween(0,pronoun.size()-1)] + ' '
         + adverb[randBetween(0,adverb.size()-1)] + ' '
         + verb[randBetween(0,verb.size()-1)] + ' '
         + preposition[randBetween(0,preposition.size()-1)] + " the "
         + adjective[randBetween(0,adjective.size()-1)] + ' '
         + noun[randBetween(0,noun.size()-1)] + ". ";
     d.pop_back();
     return d;
  };

  string json_entry( R"(
{
  "author": ")" + pronoun[randBetween(0,pronoun.size()-1)]+' '+lastname + R"(",
  "title": ")" + product + R"(",
  "description": ")" + sentence(10) + R"(",
  "category": ")" + thing() + R"(",
  "price": )" + std::to_string( randBetween( 0.0, 11.0 ) ) + R"(,
  "condition": ")" + adj() + R"(",
  "shipping": "pickup, delivery, convert to array",
  "format": "buy it now",
  "location": "home",
  "keywords": ")" + keyword(4) + R"(",
  "id": )" + std::to_string( randBetween( 0, RAND_MAX) ) + R"(
})" );

  cout << json_entry << '\n';

  return 0;
}
