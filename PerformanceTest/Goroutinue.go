package main

import (
	"fmt"
	"os"
	"strconv"
	"time"
)

func runCallback(in, out chan int64) {
	for n, ok := <-in; ok; n, ok = <-in {
		out <- n
	}
}

func runTest(round int, coroutineNum, switchTimes int64) {
	fmt.Printf("##### Round: %v\n", round)
	start := time.Now()
	channelsIn, channelsOut := make([]chan int64, coroutineNum), make([]chan int64, coroutineNum)

	for i := int64(0); i < coroutineNum; i++ {
		channelsIn[i] = make(chan int64, 1)
		channelsOut[i] = make(chan int64, 1)
	}

	end := time.Now()
	fmt.Printf("Create %v goroutines and channels cost %vns, avg %vns\n", coroutineNum, end.Sub(start).Nanoseconds(), end.Sub(start).Nanoseconds()/coroutineNum)

	start = time.Now()
	for i := int64(0); i < coroutineNum; i++ {
		go runCallback(channelsIn[i], channelsOut[i])
	}
	end = time.Now()
	fmt.Printf("Start %v goroutines and channels cost %vns, avg %vns\n", coroutineNum, end.Sub(start).Nanoseconds(), end.Sub(start).Nanoseconds()/coroutineNum)

	var sum int64 = 0
	start = time.Now()
	for i := int64(0); i < switchTimes; i++ {
		for j := int64(0); j < coroutineNum; j++ {
			channelsIn[j] <- 1
			sum += <-channelsOut[j]
		}
	}
	end = time.Now()
	fmt.Printf("Switch %v goroutines for %v times cost %vns, avg %vns\n", coroutineNum, sum, end.Sub(start).Nanoseconds(), end.Sub(start).Nanoseconds()/sum)

	start = time.Now()
	for i := int64(0); i < coroutineNum; i++ {
		close(channelsIn[i])
		close(channelsOut[i])
	}
	end = time.Now()
	fmt.Printf("Close %v goroutines cost %vns, avg %vns\n", coroutineNum, end.Sub(start).Nanoseconds(), end.Sub(start).Nanoseconds()/coroutineNum)
}

func main() {
	var coroutineNum, switchTimes int64 = 30000, 1000

	fmt.Printf("### Run: ")
	for _, v := range os.Args {
		fmt.Printf(" \"%s\"", v)
	}
	fmt.Printf("\n")

	if (len(os.Args)) > 1 {
		v, _ := strconv.Atoi(os.Args[1])
		coroutineNum = int64(v)
	}

	if (len(os.Args)) > 2 {
		v, _ := strconv.Atoi(os.Args[2])
		switchTimes = int64(v)
	}

	for i := 1; i <= 5; i++ {
		runTest(i, coroutineNum, switchTimes)
	}
}

// 从测试数据来看Go语言内部一定是对于Goroutinue以及channel是有一种缓冲机制的，每次都是第一组比较慢，后面比较快，所以测试数据去掉一个最高。去掉一个最低剩下取平均