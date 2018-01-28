#include <stdio.h>

int main(int argc, char const *argv[]) {
  int numbers;
  for(numbers = 0; numbers <= 30; numbers++){
    if((numbers % 3) != 0 && numbers % 5 != 0) {
        printf("%d\n", numbers);
      }
  }
  return 0;
}
