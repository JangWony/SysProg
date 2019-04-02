import java.io.*;

enum Planet {

    MERCURY(1,1),
    VENUS(2,2);

    private final double mass;
    private final double radius;
    Planet(double mass, double radius){
        this.mass = mass;
        this.radius = radius;
    }
    public double get_mass(){
        return mass;
    }
}

enum Color{
    red, blue, green
}
enum TrafficLight{
    green, yellow, red
}

public  class test {
    public static void main(String[] args) {
        
        class Vector{
            int sz;
            double[] elem;

            public Vector(){
                sz=0;
                elem = null;
            }

            public Vector(int s){
                sz=s;
                elem = new double[s];
            }
            public Vector(int s, double[] elem){


            }
            public int get_sz(){
                return sz;
            }
        }
        Vector V = new Vector(3);
        System.out.println(V.get_sz());
        System.out.println(TrafficLight.red);   

        Planet.MERCURY();
    }
}