//Hanyu Xiong
//CS1550 project 3

public class Page {
	private String memLoc; //Memory name
	public boolean memoryRead, memoryWrite; //If it has been reference or modified respectively
	public boolean op; //0(false) is a read, 1(true) is a write.
	private int position; 
	
	public Page(String mem, String operation, int pos){
		memLoc = mem;
		position = pos;
		memoryRead = false;
		memoryWrite = false;
		String oper = operation.toLowerCase();
		if(oper.equals("w")){
			op = true;
		}
		else{
			op = false;
		}
	}
	
	public void setRead(boolean readOrNot){
		memoryRead = readOrNot;
	}
	public void setMod(boolean writeOrNot){
		memoryWrite = writeOrNot;
	}
	public boolean isModified(){
		return memoryWrite;
	}
	public boolean isRead(){
		return memoryRead;
	}
	public String getMemLocation(){
		return memLoc;
	}
	public int getPosition(){
		return position;
	}
}
