#include <bits/stdc++.h>
using namespace std;

            static const constexpr size_t FirstSlot = 8*1024;
            static const constexpr size_t SecondSlot = 32*1024;
            static const constexpr size_t ThirdSlot = 128*1024; 

// 运行此函数前需要保证传入的值是有效的范围，即[0, ThirdSlot]
int exchange(int length){
    if(length <= FirstSlot){
        if(length & 0b111111111){
            length &= ~0b111111111;
            length += 0b1000000000;
        }
    } else if(length <= SecondSlot){
        if(length & 0b1111111111){
            cout << "second\n";
            length &= ~0b1111111111;
            cout << length << endl;
            length += 0b10000000000;
        } 
    } else if(length <= ThirdSlot){
        if(length & 0b11111111111){
            length &= ~0b11111111111;
            cout << length << endl;
            length += 0b100000000000;
        } 
    }

    if(length <= FirstSlot){
        length >>= 9;   // 除以512
    } else if (length <= SecondSlot){
        length >>= 10;    // 第二阶段的槽大小为1KB
        length += 8;
    } else if (length <= ThirdSlot){
        length >>= 11;
        length += 24;
    }

    // 槽数以开始计算
    return length - 1;
}

int main(){
    int temp = 0;
    cin >> temp;
    cout << exchange(temp) << endl;;
    return 0;
}