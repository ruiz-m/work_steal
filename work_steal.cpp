#include <iostream>
#include <omp.h>
#include <deque>
#include <vector>
#include <unistd.h>
#include <chrono>
#include <time.h>
#include <thread>

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
			break;
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
			//std::cout << "Deleting plus\n";
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
			break;
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
		bool depDone();
		void setRes(Typ ty);
		Typ getRes();
		std::vector<Thr*> &getDep();
		void setDep(std::vector<Thr*> &v);
		bool depEmpty();
		void set(Thr &th);
		~Thr()
		{
			//std::cout << "deleting thr\n"; 
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

bool Thr::depDone()
{
	bool res = true;
	for(int i=0; i<dep.size(); ++i)
	{
		res = res && dep[i]->getDone();
	}
	return res;
}

static int num_thr;
static int count = 0;
static int work_steal_count =  0;
omp_lock_t count_lock;

int getCount()
{
	omp_set_lock(&count_lock);
	int res = count;
	omp_unset_lock(&count_lock);
	return res;
}

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
		Thr *dq_back(int i);
		Thr *dq_pop_front(int i);
		void process_thr();
		Thr *spawn_thr(Exp *ex, Mode m);
		void join_thr(std::vector<Thr*> &v, Mode m);
		void end_thr(Typ res);
		void stall_thr();
		Thr *work_steal();
		
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

Thr *Proc::dq_back(int i)
{
	return (*pool)[i].back();
}

Thr *Proc::dq_pop_front(int i)
{
	Thr *t = (*pool)[i].front();
	(*pool)[i].pop_front();
	return t;
}

static int D = 0;

void Proc::process_thr()
{
	while(count != 0)
	{
		Mode m = current->getMode();
		switch(m.cl)
		{
			case Mode_cl::Syn:
			{
				synth(current->getExp());
				break;
			}
			case Mode_cl::Plus:
			{
				std::vector<Thr*> dep = current->getDep();
				if(dep[0]->getDone() && dep[1]->getDone())
				{
					synth_plus(dep[0]->getRes(), dep[1]->getRes());
					delete dep[0];
					delete dep[1];
				}
				else
				{
					//std::cout << "Time to stall\n";
					stall_thr();
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
}

Thr *Proc::spawn_thr(Exp *ex, Mode m)
{
	omp_set_lock(&count_lock);
	++count;
	omp_unset_lock(&count_lock);
	return new Thr(ex, m);
}

void Proc::join_thr(std::vector<Thr*> &v, Mode m)
{
	current->setDep(v);
	current->setMode(m);
	omp_set_lock(&((*dq_lock)[id]));
	dq_push_back(id, v[1]);
	dq_push_back(id, v[0]);	
	omp_unset_lock(&((*dq_lock)[id]));
}

void Proc::end_thr(Typ ty)
{
	current->setRes(ty);
	current->setDone(true);
	//printf("%d = %x\n", id, &((*dq_lock)[id]));
	
	omp_set_lock(&count_lock);
	--count;
	if(count == 0)
	{
		omp_unset_lock(&count_lock);
		return;
	}
	omp_unset_lock(&count_lock);
	
	omp_set_lock(&((*dq_lock)[id]));
	if((*pool)[id].empty())
	{
		//std::cout << id << " work stealing with count=" << count << "\n";
		omp_unset_lock(&((*dq_lock)[id]));
		while(count != 0) 
		{
			//work steal
			int vict = rand() % num_thr;
			omp_set_lock(&((*dq_lock)[vict]));
			if(!(*pool)[vict].empty())
			{
				current = dq_pop_front(vict);
				omp_unset_lock(&((*dq_lock)[vict]));
				++work_steal_count;
				break;
			}
			omp_unset_lock(&((*dq_lock)[vict]));
		}
	}
	else
	{
		current = dq_pop_back(id);
		//std::cout << id << " New current\n";
		//current->getExp()->printCl();
		omp_unset_lock(&((*dq_lock)[id]));
	}
}

void Proc::stall_thr()
{
	omp_set_lock(&((*dq_lock)[id]));
	if((*pool)[id].empty())
	{
		omp_unset_lock(&((*dq_lock)[id]));
		while(count != 0 && !current->depDone())
		{
			//work steal
			int vict = rand() % num_thr;
			omp_set_lock(&((*dq_lock)[vict]));
			if(!(*pool)[vict].empty())
			{
				dq_push_back(id, current);
				current = dq_pop_front(vict);
				omp_unset_lock(&((*dq_lock)[vict]));
				++work_steal_count;
				break;
			}
			omp_unset_lock(&((*dq_lock)[vict]));
		}
	}
	else
	{
		/*Thr *thr = dq_pop_back(id);
		dq_push_back(id, current);
		omp_unset_lock(&((*dq_lock)[id]));
		current = thr;*/
		omp_unset_lock(&((*dq_lock)[id]));
		while(1)
		{
			omp_set_lock(&((*dq_lock)[id]));
			if(!(*pool)[id].empty())
			{
				Thr *thr = dq_back(id);
				if(thr->depDone())
				{
					dq_pop_back(id);
					dq_push_back(id, current);
					omp_unset_lock(&((*dq_lock)[id]));
					current = thr;
					break;
				}
			}
			omp_unset_lock(&((*dq_lock)[id]));
			if(current->depDone())
			{
				break;
			}
		}
	}
}

Thr *Proc::work_steal()
{
	Thr *t = nullptr;
	int vict = rand() % num_thr;
	//vict = (vict == id ? (vict+1)%num_thr : vict);
	omp_set_lock(&((*dq_lock)[vict]));
	if(!(*pool)[vict].empty())
	{
		//printf("%d stole %d\n", id, vict);
		t = dq_pop_front(vict);
		++work_steal_count;
		//t->getExp()->printCl();
	}
	omp_unset_lock(&((*dq_lock)[vict]));
	return t;
}

void Proc::synth(Exp *exp)
{
	switch(exp->getCl())
	{
		case Exp_cl::Bool:
		{
		        end_thr(Typ::Bool);
			break;
		}
		case Exp_cl::Int:
		{
			//std::cout << "SYNTH END INT\n";
			end_thr(Typ::Int);
			break;
		}
		case Exp_cl::Plus:
		{
			//std::cout << "SYNTH PLUS\n";
			Plus *ex = dynamic_cast<Plus*>(exp);
			Thr *thr1 = spawn_thr(ex->getE1(), Mode(Mode_cl::Syn, Typ::None));
			Thr *thr2 = spawn_thr(ex->getE2(), Mode(Mode_cl::Syn, Typ::None));
			std::vector<Thr*> v = {thr1, thr2};
			join_thr(v, Mode(Mode_cl::Plus, Typ::None));
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

Exp *example_exp(int d)
{
	if(d == 1)
	{
		return new Exp(Exp_cl::Int);
	}
	else
	{
		Exp *e1 = example_exp(d-1);
		Exp *e2 = example_exp(d-1);
		return new Plus(e1, e2);
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
	omp_init_lock(&count_lock);
	srand(time(NULL));
	std::cout << "num_thr=" << num_thr << "\n";
	
	Exp *p1 = example_exp(20);
	++count;
	Thr *thr = new Thr(p1, Mode(Mode_cl::Syn, Typ::None));
	bool sk = true;
	#pragma omp parallel shared(pool, dq_lock)
	{
		int id = omp_get_thread_num();

		Proc p = Proc(id, &pool, &dq_lock);
		if(id == 0)
		{
			auto start = std::chrono::high_resolution_clock::now();
			p.set_current(thr);
			p.process_thr();
			auto finish = std::chrono::high_resolution_clock::now();
			auto mil = std::chrono::duration<double, std::milli>(finish - start).count();
			std::cout << "Time=" << mil << "\n";
			std::cout << work_steal_count << "\n";
			printTyp(thr->getRes());
		}
		else
		{
			while(sk)
			{
				omp_set_lock(&(dq_lock[0]));
				if(!pool[0].empty())
				{
					p.set_current(p.dq_pop_front(0));
					++work_steal_count;
					sk = false;
					omp_unset_lock(&(dq_lock[0]));
					p.process_thr();
					break;
				}
				omp_unset_lock(&(dq_lock[0]));
			}

			while(count != 0)
			{
				int vict = rand() % num_thr;
				omp_set_lock(&(dq_lock[vict]));
				if(!pool[vict].empty())
				{
					//printf("%d stole %d\n", id, vict);
					//printf("SIZE=%d\n", pool.size());
					p.set_current(p.dq_pop_front(vict));
					omp_unset_lock(&(dq_lock[vict]));
					++work_steal_count;
					p.process_thr();
					break;
				}
				omp_unset_lock(&(dq_lock[vict]));
			}
		}
	}
	
	delete thr;
	delete p1;
	for(int i=0; i<num_thr; ++i)
	{
		omp_destroy_lock(&dq_lock[i]);
	}
	omp_destroy_lock(&count_lock);
	return 0;
}
