#include <iostream>
#include <omp.h>
#include <deque>
#include <vector>
#include <unistd.h>

enum struct Exp_cl
{
	Bool,
	Int,
	Plus
};

class Exp
{
	private:
		Exp_cl cl;

	public:
		Exp(Exp_cl cl) : cl(cl) {}
		Exp_cl getCl();
		void printCl();
		virtual ~Exp() {}
};

Exp_cl Exp::getCl()
{
	return cl;
}

void Exp::printCl()
{
	switch(cl)
	{
		case Exp_cl::Bool:
			std::cout << "Bool\n";
			break;
		case Exp_cl::Int:
			std::cout << "Int\n";
			break;
		case Exp_cl::Plus:
			std::cout << "Plus\n";
			break;
		default:
			std::cout << "printCl Error\n";
			exit(1);
	}
}

class Plus : public Exp
{
	private:
		Exp *e1;
		Exp *e2;

	public:
		Plus(Exp *exp1, Exp *exp2) : Exp(Exp_cl::Plus), e1(exp1), e2(exp2) {}
		~Plus()
		{
			std::cout << "Deleting plus\n";
			delete e1;
			delete e2;
		}
		Exp *getE1();
		Exp *getE2();
};

Exp *Plus::getE1()
{
	return e1;
}

Exp *Plus::getE2()
{
	return e2;
}

enum struct Typ
{
	None,
	Bool,
	Int
};

void printTyp(Typ ty)
{
	switch(ty)
	{
		case Typ::None:
			std::cout << "None\n";
			break;
		case Typ::Bool:
			std::cout << "Bool\n";
			break;
		case Typ::Int:
			std::cout << "Int\n";
			break;
		default:
			std::cout << "printTyp Error\n";
			exit(1);
	}
}

enum struct Mode_cl
{
	Ana,
	Syn,
	Plus
};

struct Mode
{
	Mode_cl cl;
	Typ ty;

	Mode(Mode_cl cl, Typ ty) : cl(cl), ty(ty) {}
	Mode &operator=(Mode &m)
	{
		cl = m.cl;
		ty = m.ty;
		return *this;
	}
};

class Thr
{
	//context for the future
	private:
		Exp *exp;
		Mode mode;
		bool done;
		Typ res;
		std::vector<Thr*> dep;
	public:
		Thr(Exp *exp, Mode mode) : exp(exp), mode(mode), done(false), dep(std::vector<Thr*>()) {}
		Thr(std::vector<Thr*> &dep, Mode mode) : exp(nullptr), dep(dep), mode(mode), done(false) {}
		Exp *getExp();
		Mode getMode();
		void setMode(Mode m);
		void setDone(bool q);
		bool getDone();
		void setRes(Typ ty);
		Typ getRes();
		std::vector<Thr*> &getDep();
		void setDep(std::vector<Thr*> &v);
		bool depEmpty();
		void set(Thr &th);
		~Thr()
		{
			std::cout << "deleting thr\n"; 
		}
};

Exp* Thr::getExp()
{
	return exp;
}

Mode Thr::getMode()
{
	return mode;
}

void Thr::setMode(Mode m)
{
	mode = m;
}

void Thr::setDone(bool q)
{
	done = q;
}

bool Thr::getDone()
{
	return done;
}

void Thr::setRes(Typ ty)
{
	res = ty;
}

Typ Thr::getRes()
{
	return res;
}

std::vector<Thr*> &Thr::getDep()
{
	return dep;
}

void Thr::setDep(std::vector<Thr*> &d)
{
	dep = d;
}

bool Thr::depEmpty()
{
	return dep.empty();
}

static int num_thr;
static int count = 0;

class Proc
{
	private:
		int id;
		std::vector<std::deque<Thr*>> *pool;
		std::vector<omp_lock_t> *dq_lock;
		Thr *current;

	public:
		Proc(int id, std::vector<std::deque<Thr*>> *pool, std::vector<omp_lock_t> *dq_lock) : id(id), pool(pool), dq_lock(dq_lock), current(nullptr) {}

		void set_current(Thr *thr);
		void dq_push_back(int i, Thr *thr);
		Thr *dq_pop_back(int i);
		Thr *dq_pop_front(int i);
		void process_thr();
		Thr *spawn_thr(Exp *ex, Mode m);
		void join_thr(std::vector<Thr*> &v, Mode m);
		void end_thr(Typ res);
		void stall_thr();
		bool work_steal();
		
		void synth(Exp *exp);
		void synth_plus(Typ ty1, Typ ty2);
};

void Proc::set_current(Thr *thr)
{
	current = thr;
}

void Proc::dq_push_back(int i, Thr *thr)
{
	(*pool)[i].push_back(thr);
}

Thr *Proc::dq_pop_back(int i)
{
	Thr *t = (*pool)[i].back();
	(*pool)[i].pop_back();
	return t;
}

Thr *Proc::dq_pop_front(int i)
{
	Thr *t = (*pool)[i].front();
	(*pool)[i].pop_front();
	return t;
}

void Proc::process_thr()
{
	std::cout << id << " process thr\n";
	current->getExp()->printCl();
	Mode m = current->getMode();

	switch(m.cl)
	{
		case Mode_cl::Syn:
		{
			std::cout << "MODE SYN\n";
			synth(current->getExp());
			break;
		}
		case Mode_cl::Plus:
		{
			std::cout << "MODE PLUS\n";
			std::vector<Thr*> dep = current->getDep();
			if(dep[0]->getDone() && dep[1]->getDone())
			{
				synth_plus(dep[0]->getRes(), dep[1]->getRes());
				delete dep[0];
				delete dep[1];
			}
			else
			{
				stall_thr();
				process_thr();
			}
			break;
		}
		default:
		{
			std::cout << "Mode Error\n";
			exit(1);
			break;
		}
	}
}

Thr *Proc::spawn_thr(Exp *ex, Mode m)
{
	++count;
	return new Thr(ex, m);
}

void Proc::join_thr(std::vector<Thr*> &v, Mode m)
{
	current->setDep(v);
	current->setMode(m);
	omp_set_lock(&((*dq_lock)[id]));
	dq_push_back(id, current);
	dq_push_back(id, v[1]);
	omp_unset_lock(&((*dq_lock)[id]));
	current = v[0];
}

void Proc::end_thr(Typ ty)
{
	--count;
	std::cout << "End\n";
	current->getExp()->printCl();
	current->setRes(ty);
	current->setDone(true);
	omp_set_lock(&((*dq_lock)[id]));
	if((*pool)[id].empty() && count != 0)
	{
		work_steal();
	}
	else if(count != 0)
	{
		current = dq_pop_back(id);
		std::cout << "New current\n";
		current->getExp()->printCl();
	}
	omp_unset_lock(&((*dq_lock)[id]));
}

void Proc::stall_thr()
{
	omp_set_lock(&((*dq_lock)[id]));
	if((*pool)[id].empty() && count != 0)
	{
		std::cout << "time to work steal again\n";
		dq_push_back(id, current);
		work_steal();
	}
	else if(count != 0)
	{
		Thr *thr = dq_pop_back(id);
		dq_push_back(id, current);
		current = thr;
	}
	omp_unset_lock(&((*dq_lock)[id]));
}

static int D = 0;

bool Proc::work_steal()
{
	std::cout << id << " work stealing with count=" << count << "\n"; 
	int vict;
	++D;
	while(count != 0)
	{
		vict = rand() % num_thr;
		vict = (vict == id ? (vict+1)%num_thr : vict);
		std::cout << vict << " victim size=" << (*pool)[vict].size() << std::endl;
		omp_set_lock(&((*dq_lock)[vict]));
		std::cout << "deadlock\n";
		if(!(*pool)[vict].empty())
		{
			std::cout << id << " work stole\n"; 
			if(D == 2)
			{
				exit(1);
			}
			current = dq_pop_front(vict);
			omp_unset_lock(&((*dq_lock)[vict]));
			return true;
		}
		omp_unset_lock(&((*dq_lock)[vict]));
	}
	return false;
}

void Proc::synth(Exp *exp)
{
	std::cout << "SYNTH\n";
	switch(exp->getCl())
	{
		case Exp_cl::Bool:
		{
		        end_thr(Typ::Bool);
			process_thr();
			break;
		}
		case Exp_cl::Int:
		{
			std::cout << "SYNTH END INT\n";
			end_thr(Typ::Int);
			process_thr();
			break;
		}
		case Exp_cl::Plus:
		{
			std::cout << "SYNTH PLUS\n";
			Plus *ex = dynamic_cast<Plus*>(exp);
			Thr *thr1 = spawn_thr(ex->getE1(), Mode(Mode_cl::Syn, Typ::None));
			Thr *thr2 = spawn_thr(ex->getE2(), Mode(Mode_cl::Syn, Typ::None));
			std::vector<Thr*> v = {thr1, thr2};
			join_thr(v, Mode(Mode_cl::Plus, Typ::None));
			sleep(1);
			process_thr();
			break;
		}
		default:
		{
			std::cout << "Invalid expression\n";
			exit(1);
			break;
		}
	}
}

void Proc::synth_plus(Typ ty1, Typ ty2)
{
	if(ty1 == Typ::Int && ty2 == Typ::Int)
	{
		end_thr(Typ::Int);
	}
	else
	{
		end_thr(Typ::None);
	}
}

int main(int argc, char **argv)
{
	num_thr = omp_get_max_threads();
	std::vector<std::deque<Thr*>> pool(num_thr);
	std::vector<omp_lock_t> dq_lock(num_thr);
	for(int i=0; i<num_thr; ++i)
	{
		omp_init_lock(&dq_lock[i]);
	}
	std::cout << num_thr << "\n";
	
	bool began = false;
	#pragma omp parallel shared(pool, began)
	{
		int id = omp_get_thread_num();
		std::cout << id << "\n";

		Proc p = Proc(id, &pool, &dq_lock);
		if(id == 0)
		{
			Exp *e1 = new Exp(Exp_cl::Int);
			Exp *e2 = new Exp(Exp_cl::Int);
			//Exp *e3 = new Exp(Exp_cl::Int);
			//Exp *e4 = new Exp(Exp_cl::Int);
			Exp *p1 = new Plus(e1, e2);
			//Exp *p2 = new Plus(e3, e4);
			//Exp *p3 = new Plus(p1, p2);
			Thr* thr = p.spawn_thr(p1, Mode(Mode_cl::Syn, Typ::None));
			began = true;
			p.set_current(thr);
			p.process_thr();
			
			std::cout << "Yabo\n";
			printTyp(thr->getRes());
			delete thr;
			delete p1;
		}
		else
		{
			while(!began) {}
			if(p.work_steal())
			{
				p.process_thr();
			}
		}
	}

	for(int i=0; i<num_thr; ++i)
	{
		omp_destroy_lock(&dq_lock[i]);
	}
	return 0;
}
