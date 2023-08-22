public class HelloWorld {
	public static void say_hello() {
		System.out.println("Javaaaa!!!");
	}

	public static void say_number(int number) {
		System.out.println(number);
	}
	
	public static void main(String[] args) {
		int a = 12345;
		
		System.out.println("Hello, I'm Zeraora!");
		System.out.println("This is ZeraVM, Java Virtual Machine made by NDRAEY!");
		say_number(a);
		
		say_hello();
	}
}
