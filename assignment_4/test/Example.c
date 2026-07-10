void loops() {
  int array[10];

  for (int i = 0; i < 10; i++) {
    array[i] = 3;
  }

  int x;
  for (int i = 0; i < 10; i++) {
    x = array[i-1];
  }
}
