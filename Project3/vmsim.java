//Hanyu Xiong
//CS1550 project 3

/*
compile with:
	javac vmsim.java
run with:
	java vmsim –n <numframes> -a <opt|clock|nru|rand> [-r <refresh>] <tracefile>

*/

import java.io.File;
import java.io.FileReader;
import java.io.BufferedReader;
import java.util.Random;
import java.util.HashMap;
import java.util.LinkedList;
import java.io.IOException;



public class vmsim {
	static HashMap<String, Page> pageTable= new HashMap<String, Page>();		//page table hashmap
	static HashMap<String, LinkedList<Page>> opMoves = new HashMap<String, LinkedList<Page>>();	//opmoves hashmap for opt algorithm
	static LinkedList<Page> instructions= new LinkedList<Page>();		//linkedlist for instructions
	static LinkedList<Page> clockQueue = new LinkedList<Page>();		//linked list for queue for clock algorithm
	static int numFrames, pageFaults, writes, memAccesses, clockHand, refreshSize;	//declare int values usef throughtout the program
	static String alg;		//the algorithm that is chosen
	static Page removeNode = null;		//node to be removed for each of the algorithms 
	private static BufferedReader readfile;	//the file to be read
	
	public static void main(String[] args) throws IOException{
		String algorithm = null, tracefile = null;
		numFrames = 0; 			
		//Check input arguments: 
		for(int i = 0; i < args.length; i++){
			if ((i+1 < args.length)){
				if(args[i].equals("-r") && args[i-1].equals("nru") ){	//get the integer after -n if possible
					if (i+2 < args.length){
						tracefile = args[i+2];
					}
					refreshSize = Integer.parseInt(args[i+1]);
				}
				if(args[i].equals("-a")){		//only get the algorithm and tracefile if there's 2 arguments after -a
					if (i+3 == args.length){
						tracefile = args[i+2];
					}
					algorithm = args[i+1];
				}
				if(args[i].equals("-n")){	//get the integer after -n if possible
					String numframes = args[i+1];
					numFrames = Integer.parseInt(numframes);
				}
			}
		}
		alg = algorithm.toLowerCase();
		//error message if inputs are wrong
		if((args.length!=5 && args.length!=7) || numFrames == 0 ||  tracefile == null || alg == null || (refreshSize<1) && alg.equals("nru")){	//if size is wrong, or other inputs are wrong
			System.out.println("Please enter valid arguments");
			System.exit(0);
		}
		//final check for algorithm choice
		if (alg.equals("opt") || alg.equals("random") || alg.equals("nru") || (alg.equals("clock"))){
			initialize(tracefile);
			memAccesses = instructions.size();
			Algorithm();
		}
		else {
			System.out.println("Please enter valid arguments");
		}
	}
	
	//initialize
	private static void initialize(String file) throws IOException{
		int position = 0;
		readfile = new BufferedReader(new FileReader(new File(file)));
		while(readfile.ready()){
			String line[] = readfile.readLine().split(" "); 
			Page currNode = new Page(line[0], line[1], position);
			if(alg.equals("opt")){
				position++;
				addOp(line[0], currNode);
			}
			instructions.add(currNode);		
		}
	}
	
	//algorithm that covers opt, random, nru, clock
	public static void Algorithm(){
		if(instructions.size() == 0){
			return;
		}
		Page currNode = null;
		int position = 0;		
		
		while(instructions.size() > 0){	//while there are still instructions
			currNode = instructions.remove();	//remove first instruction as current node
			currNode.setRead(true); 	//set read as true
			if(alg.equals("opt")) {	//opt 
				LinkedList<Page> list = opMoves.get(currNode.getMemLocation());	
				list.remove();													
			}
			if(alg.equals("nru")){		//nru					
				position++;												
				if(position %refreshSize== 0){							
					for(String currPage: pageTable.keySet()){		
						pageTable.get(currPage).setRead(false);		
					}													
				}														
			}
			if(currNode.op){
				currNode.setMod(true);	//set modified as true
			}		
			if(pageTable.containsKey(currNode.getMemLocation())){	//a hit
				System.out.println("hit");
				if(!alg.equals("opt")){	//nru,clock,random
					pageTable.get(currNode.getMemLocation()).setRead(true); 		
				}
			}
			else if(pageTable.size() >= numFrames){		//page fault
				pageFaults++;
				if(alg.equals("nru")){
					removeNRU();		//nru
				}
				if(alg.equals("opt")){	//opt
					removeOpt();		
				}
				if(alg.equals("random")){
					removeRandom();	//random
				}
				if(alg.equals("clock")){//clock
					Page removeFromTable = swapClock(currNode);		
					pageTable.remove(removeFromTable.getMemLocation());	
				}
				pageTable.put(currNode.getMemLocation(), currNode);
			}
			else{		//otherwise, page fault but no eviction
				System.out.println("page fault – no eviction");	
				pageFaults++;
				pageTable.put(currNode.getMemLocation(), currNode);
				if(alg.equals("clock"))	//clock
					clockQueue.add(currNode);						
			}
		}
		//output results
		outputResults();
	}
	
	//remove node for opt algorithm
	private static void removeOpt(){
		removeNode = null;
		for(String currLocation: pageTable.keySet()){
			if(opMoves.get(currLocation).isEmpty()){
				removeNode = pageTable.remove(currLocation);
				declarePageFault(removeNode);
				return;
			}
			else if(removeNode == null || removeNode.getPosition() < opMoves.get(currLocation).get(0).getPosition()){
				removeNode = opMoves.get(currLocation).get(0);
			}
		}
		declarePageFault(removeNode);		//declare the page fault for the removed node
		pageTable.remove(removeNode.getMemLocation());	//remove the node from the page table
	}
	//remove node for random algorithm
	private static void removeRandom(){
		removeNode = null;
		Random rand = new Random();
		int  randInt = rand.nextInt(pageTable.size());	//generate random number in table size
		int counter=0;
		for(Page currPage: pageTable.values()){	//go though page table values
			if (randInt==counter){			//remove page table value corresponding to random value
				removeNode = currPage;
				break;
			}
			counter++;
		}	
		declarePageFault(removeNode);	//declare the page fault for the removed node
		pageTable.remove(removeNode.getMemLocation());	//remove the node from the page table
	}
	//remove node for nru algorithm
	private static void removeNRU(){
		removeNode = null;
		Page[] type = new Page[3];
		for(Page currPage: pageTable.values()){	//go through all the page table values 
			if(!currPage.isModified() && !currPage.isRead()){	//if node is not modified or read then remove it
				removeNode = currPage;
				break;
			}
			if(currPage.isModified() && currPage.isRead()){	//if node is modified and read then make it last option
				type[2] = currPage;
			}
			else if(!currPage.isModified() && currPage.isRead()){	//if node is not modified, but is read then make it 2nd last option
				type[1] = currPage;
			}
			else if(currPage.isModified() && !currPage.isRead()){	//if node is modified but not read then make it first option
				type[0] = currPage;
			}
		}		
		if(removeNode == null){	//couldn't find a node not modified or read	go to next options
			if(type[0]!= null){		//first option if not null use it
				removeNode = type[0];
			}
			else if(type[1]!=null){	//otherwise second option if not null use it
				removeNode = type[1];
			}
			else{			//finally use lasat option
				removeNode = type[2];
			}
		}
		declarePageFault(removeNode);	//declare the page fault for the removed node
		pageTable.remove(removeNode.getMemLocation());	//remove the node from the page table
	}
	//adding moves for the opt algorithm
	private static void addOp(String memName, Page currNode){
		if(!opMoves.containsKey(memName)){		//if opmoves doesn't contain the key 
			LinkedList<Page> temp = new LinkedList<Page>();
			temp.add(currNode);
			opMoves.put(memName, temp);
		}
		else{			//if opmoves does contain the key 
			if(opMoves.get(memName) == null){		//if it's null then put the new linked list with currNode into it
				LinkedList<Page> temp = new LinkedList<Page>();
				temp.add(currNode);
				opMoves.put(memName, temp);
			}
			else{					//otherwise add currNode to it
				opMoves.get(memName).add(currNode);
			}
		}
	}
	//swapping the page replacement
	private static Page swapClock(Page replacement){	
		Page currNode = clockQueue.get(clockHand);
		while(currNode.isRead()){	//while current node has been read
			clockHand = (clockHand+1) % numFrames;
			currNode.setRead(false);		//set read as false
			currNode = clockQueue.get(clockHand);	//get next node from queue
		}
		declarePageFault(currNode);		//declare page fault for the node
		clockQueue.set(clockHand, replacement);		//set the node at the clockhand as the replacement page
		clockHand = (clockHand+1) % numFrames;
		return currNode;
	}
	//outputting the final data results
	private static void outputResults(){
		System.out.println(   "Number of frames:      " + numFrames +"\n"
							+ "Total Memory Access:   " + memAccesses + "\n"
							+ "Total Page Faults:     " + pageFaults + "\n"
							+ "Total Writes To Disk:  " + writes);
	}
	//declaring pge fault for evicting dirty or clean
	private static void declarePageFault(Page node){
		if(node.isModified()){
			System.out.println("page fault – evict dirty");
			writes++;
		}
		else{
			System.out.println("page fault – evict clean");
		}		
	}
}