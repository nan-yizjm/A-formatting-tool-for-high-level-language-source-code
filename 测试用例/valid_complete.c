#include <stdio.h>
#define N 10

// 输出一个整数
int printValue(int value);

int main(void)
{
    int values[N];
    int i;

    for (i = 0; i < N; i = i + 1)
    {
        values[i] = i * 2;
    }

    if (N > 0 && values[0] < values[1])
    {
        printValue(values[1]);
    }

    return 0;
}

int printValue(int value)
{
    printf("%d", value);
    return value;
}
